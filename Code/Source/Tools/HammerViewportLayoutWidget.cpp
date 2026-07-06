#include "HammerViewportLayoutWidget.h"
#include "HammerWidget.h"

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraControllerRequestBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/std/algorithm.h>
#include <AzFramework/Viewport/CameraState.h>
#include <AzQtComponents/Components/DockMainWindow.h>
#include <AzQtComponents/Components/DockTabWidget.h>
#include <AzQtComponents/Components/FancyDocking.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>
#include <Editor/EditorViewportSettings.h>

#include <QAction>
#include <QEvent>
#include <QFile>
#include <QIcon>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace Hammer
{
    HammerViewportLayoutWidget::HammerViewportLayoutWidget(QWidget* parent)
        : QWidget(parent)
    {
        AZ_Assert(
            !HammerEditorActiveViewportRequestBus::HasHandlers(),
            "HammerViewportLayoutWidget constructed while another HammerEditorActiveViewportRequestBus handler exists");
        AZ_Assert(parent, "HammerViewportLayoutWidget expects the view pane to provide a parent widget");

        QFile styleSheetFile(QStringLiteral(":/Hammer/Hammer.qss"));
        const bool styleSheetLoaded = styleSheetFile.open(QFile::ReadOnly);
        AZ_Warning("HammerViewportLayoutWidget", styleSheetLoaded, "Could not load :/Hammer/Hammer.qss");
        styleSheetLoaded && (setStyleSheet(QString::fromUtf8(styleSheetFile.readAll())), true);

        auto* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);

        m_dockHost = new AzQtComponents::DockMainWindow;
        m_dockHost->setDockOptions(QMainWindow::GroupedDragging | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
        outerLayout->addWidget(m_dockHost);

        m_fancyDocking = new AzQtComponents::FancyDocking(m_dockHost, "hammerviewportdocking");

        for (int i = 0; i < MaxViewportCount; ++i)
        {
            auto* dock = new AzQtComponents::StyledDockWidget(QStringLiteral("Perspective %1").arg(i + 1), m_dockHost);
            dock->setObjectName(QStringLiteral("HammerViewportDock%1").arg(i + 1));
            dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            dock->hide();

            HammerWidget* viewport = new HammerWidget(dock);
            dock->setWidget(viewport);
            connect(
                viewport, &HammerWidget::ViewportFocusRequested, this,
                [this, viewport]
                {
                    ActivateViewport(viewport);
                });
            m_viewports.push_back(viewport);
        }

        AZ_Assert(m_viewports.size() == MaxViewportCount, "HammerViewportLayoutWidget failed to create all %d viewports", MaxViewportCount);

        m_dockAnchor = new AzQtComponents::StyledDockWidget(QStringLiteral("Viewport Dock Area"), m_dockHost);
        m_dockAnchor->setObjectName(QStringLiteral("HammerViewportDockAnchor"));
        m_dockAnchor->setFeatures(QDockWidget::NoDockWidgetFeatures);
        m_dockAnchor->setTitleBarWidget(new QWidget(m_dockAnchor));
        m_dockAnchor->setWidget(new QWidget(m_dockAnchor));
        m_dockAnchor->hide();

        m_addViewportAction = new QAction(QIcon(QStringLiteral(":/Hammer/add-view.svg")), QObject::tr("Add Hammer Viewport"), this);
        m_addViewportAction->setObjectName(QStringLiteral("HammerAddViewportButton"));
        AZ_Warning(
            "HammerViewportLayoutWidget", !m_addViewportAction->icon().isNull(),
            "add-view.svg did not load from the Hammer qrc; the add-viewport button will render blank");
        connect(
            m_addViewportAction, &QAction::triggered, this,
            [this]
            {
                auto* adoptedDock =
                    m_adoptedViewport ? qobject_cast<QDockWidget*>(m_adoptedViewport->parentWidget()) : nullptr;
                HammerWidget* unused = nullptr;
                for (HammerWidget* viewport : m_viewports)
                {
                    auto* dock = qobject_cast<QDockWidget*>(viewport->parentWidget());
                    const bool inUse = viewport == m_adoptedViewport || !dock || dock->isVisible() ||
                        AzQtComponents::DockTabWidget::IsTabbed(dock);
                    unused = (!unused && !inUse) ? viewport : unused;
                }
                (unused && adoptedDock) &&
                    (m_fancyDocking->tabifyDockWidget(
                         adoptedDock, qobject_cast<QDockWidget*>(unused->parentWidget()), m_dockHost),
                     unused->show(), true);
            });

        auto* firstDock = qobject_cast<QDockWidget*>(m_viewports[0]->parentWidget());
        AZ_Assert(firstDock, "The startup viewport is not hosted in a QDockWidget");
        m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, firstDock);
        firstDock->show();
        m_viewports[0]->SetRenderTickEnabled(true);
        ActivateViewport(m_viewports[0]);

        HammerEditorActiveViewportRequestBus::Handler::BusConnect();
        HammerViewportRequestBus::Handler::BusConnect();
        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusConnect();
        Camera::EditorCameraRequestBus::Handler::BusConnect();
    }

    HammerViewportLayoutWidget::~HammerViewportLayoutWidget()
    {
        AZ_Assert(
            HammerEditorActiveViewportRequestBus::Handler::BusIsConnected(),
            "HammerViewportLayoutWidget destroyed while its active-viewport bus was already disconnected");
        AZ_Assert(
            HammerViewportRequestBus::Handler::BusIsConnected(),
            "HammerViewportLayoutWidget destroyed while its viewport-request bus was already disconnected");

        Camera::EditorCameraRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusDisconnect();
        HammerViewportRequestBus::Handler::BusDisconnect();
        HammerEditorActiveViewportRequestBus::Handler::BusDisconnect();
    }

    void HammerViewportLayoutWidget::SetActiveViewportId(AzFramework::ViewportId viewportId)
    {
        AZ_Assert(viewportId != AzFramework::InvalidViewportId, "SetActiveViewportId called with an invalid ViewportId");
        AZ_Assert(
            AZStd::find_if(
                m_viewports.begin(), m_viewports.end(),
                [viewportId](HammerWidget* viewport)
                {
                    return viewport->GetViewportId() == viewportId;
                }) != m_viewports.end(),
            "SetActiveViewportId called with a ViewportId that no Hammer viewport owns");
        m_activeViewportId = viewportId;
    }

    AzFramework::ViewportId HammerViewportLayoutWidget::GetActiveViewportId() const
    {
        return m_activeViewportId;
    }

    void HammerViewportLayoutWidget::OnStartGameModeRequest()
    {
        AZ_Assert(m_adoptedViewport, "Game mode was requested before the real viewport was adopted");
        AZ_Assert(!m_preGameModeActiveViewport, "Game mode started while a previous pre-game-mode viewport is still pending");
        m_adoptedViewport && (m_preGameModeActiveViewport = m_activeViewport, ActivateViewport(m_adoptedViewport), true);
        for (HammerWidget* viewport : m_viewports)
        {
            viewport->SetGameModeSuppressed(true);
        }
    }

    void HammerViewportLayoutWidget::OnStopGameModeRequest()
    {
        AZ_Assert(m_adoptedViewport, "Game mode was stopped before the real viewport was adopted");
        AZ_Assert(
            !m_preGameModeActiveViewport ||
                AZStd::find(m_viewports.begin(), m_viewports.end(), m_preGameModeActiveViewport) != m_viewports.end(),
            "The pre-game-mode viewport is no longer tracked by this layout");
        for (HammerWidget* viewport : m_viewports)
        {
            viewport->SetGameModeSuppressed(false);
        }
        m_preGameModeActiveViewport && (ActivateViewport(m_preGameModeActiveViewport), true);
        m_preGameModeActiveViewport = nullptr;
    }

    void HammerViewportLayoutWidget::ActivateViewport(HammerWidget* viewport)
    {
        AZ_Assert(viewport, "ActivateViewport called with a null viewport");
        AZ_Assert(!m_viewports.empty(), "ActivateViewport called with no viewports");
        AZ_Assert(
            AZStd::find(m_viewports.begin(), m_viewports.end(), viewport) != m_viewports.end(),
            "ActivateViewport called with a viewport this layout does not own");

        const bool alreadyActive = viewport == m_activeViewport;

        (!alreadyActive && m_activeViewport) && (m_activeViewport->SetActive(false), true);
        !alreadyActive &&
            (viewport->SetActive(true), m_activeViewport = viewport,
             AzToolsFramework::SetIconsVisible(viewport == m_adoptedViewport), true);

        const HammerViewModes activeModes = viewport->GetViewModes();
        emit ActiveViewModesChanged(activeModes.m_normal, activeModes.m_wireframe, activeModes.m_overdraw);
    }

    void HammerViewportLayoutWidget::AdoptRealPerspectiveViewport(QWidget& realViewport)
    {
        AZ_Assert(!m_adoptedViewport, "AdoptRealPerspectiveViewport called more than once");
        AZ_Assert(m_activeViewport == m_viewports[0], "AdoptRealPerspectiveViewport expects slot 0 to still be the active viewport");

        HammerWidget* placeholder = m_viewports[0];
        auto* dock = qobject_cast<QDockWidget*>(placeholder->parentWidget());
        AZ_Assert(dock, "AdoptRealPerspectiveViewport expects slot 0's viewport to be hosted in a QDockWidget");
        AZ_Assert(!dock || dock->widget() == placeholder, "Slot 0's dock does not host the placeholder viewport");

        HammerWidget* adopted = HammerWidget::CreateAdopting(dock, realViewport);
        connect(
            adopted, &HammerWidget::ViewportFocusRequested, this,
            [this, adopted]
            {
                ActivateViewport(adopted);
            });

        m_viewports[0] = adopted;
        m_adoptedViewport = adopted;

        placeholder->SetRenderTickEnabled(false);
        placeholder->hide();

        dock->setWindowTitle(QObject::tr("Perspective") + QStringLiteral("   "));
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        dock->setWidget(adopted);
        adopted->show();
        realViewport.show();
        adopted->SetRenderTickEnabled(true);

        ActivateViewport(adopted);

        delete placeholder;

        ResolveViewportUiOverlayWindow();
    }

    void HammerViewportLayoutWidget::ResolveViewportUiOverlayWindow()
    {
        AZ_Assert(m_adoptedViewport, "ResolveViewportUiOverlayWindow called before the real viewport was adopted");
        AZ_Assert(!m_overlaySyncTimer, "ResolveViewportUiOverlayWindow should only start the sync timer once");

        m_viewportUiOverlayWindow = m_adoptedViewport->findChild<QWidget*>(QStringLiteral("ViewportUiWindow"));
        AZ_Warning(
            "HammerViewportLayoutWidget", m_viewportUiOverlayWindow,
            "Could not find the adopted viewport's ViewportUiWindow overlay; viewport UI will not follow the active viewport");
        m_viewportUiOverlayWindow && (m_viewportUiOverlayWindow->installEventFilter(this), true);

        m_overlaySyncTimer = new QTimer(this);
        connect(m_overlaySyncTimer, &QTimer::timeout, this, &HammerViewportLayoutWidget::SyncViewportUiOverlay);
        m_overlaySyncTimer->start(16);
    }

    void HammerViewportLayoutWidget::SetCameraMirroringEnabled(bool enabled)
    {
        AZ_Assert(!enabled || m_overlaySyncTimer, "Camera mirroring enabled before the overlay sync timer exists; mirroring is driven by that timer");
        m_cameraMirroringEnabled = enabled;
    }

    void HammerViewportLayoutWidget::SetActiveViewportViewModes(bool normal, bool wireframe, bool overdraw)
    {
        AZ_Assert(m_activeViewport, "SetActiveViewportViewModes called before any viewport was activated");
        m_activeViewport &&
            (m_activeViewport->SetViewModes(HammerViewModes{ normal, wireframe, overdraw }),
             emit ActiveViewModesChanged(normal, wireframe, overdraw), true);
    }

    void HammerViewportLayoutWidget::SyncViewportUiOverlay()
    {
        AZ_Assert(m_overlaySyncTimer, "SyncViewportUiOverlay ticked without its timer");
        AZ_Assert(m_adoptedViewport, "SyncViewportUiOverlay ticked before the real viewport was adopted");
        AZ_Assert(m_dockAnchor, "SyncViewportUiOverlay ticked before the dock anchor was constructed");

        auto* adoptedDock = qobject_cast<QDockWidget*>(m_adoptedViewport->parentWidget());
        AZ_Assert(adoptedDock, "The adopted viewport is not hosted in a QDockWidget");

        bool anyViewportDocked = false;
        for (HammerWidget* viewport : m_viewports)
        {
            auto* dock = qobject_cast<QDockWidget*>(viewport->parentWidget());
            viewport->SetRenderTickEnabled(viewport->isVisible() || viewport == m_adoptedViewport);
            anyViewportDocked = anyViewportDocked ||
                (dock && dock->isVisible() && !dock->isFloating() && dock->window() == m_dockHost->window());
        }

        (adoptedDock && !adoptedDock->isVisible() && !AzQtComponents::DockTabWidget::IsTabbed(adoptedDock) &&
         adoptedDock->window() == m_dockHost->window()) &&
            (m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, adoptedDock), adoptedDock->show(), anyViewportDocked = true, true);

        const bool adoptedDockedInHost = adoptedDock && adoptedDock->isVisible() && !adoptedDock->isFloating() &&
            adoptedDock->window() == m_dockHost->window();
        auto* adoptedTabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(adoptedDock);
        (adoptedDockedInHost && !adoptedTabWidget) && (m_fancyDocking->createTabWidget(m_dockHost, adoptedDock), true);
        adoptedTabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(adoptedDock);

        !m_addViewportButton &&
            (m_addViewportButton = new QToolButton(m_dockHost),
             m_addViewportButton->setObjectName(QStringLiteral("HammerAddViewportButton")),
             m_addViewportButton->setDefaultAction(m_addViewportAction), m_addViewportButton->setAutoRaise(true),
             m_addViewportButton->adjustSize(), m_addViewportButton->hide(), true);

        QTabBar* adoptedTabBar = adoptedTabWidget ? adoptedTabWidget->tabBar() : nullptr;
        (adoptedTabWidget && m_addViewportButton->parentWidget() != adoptedTabWidget) &&
            (m_addViewportButton->setParent(adoptedTabWidget), true);
        (!adoptedTabWidget && m_addViewportButton->parentWidget() != m_dockHost) &&
            (m_addViewportButton->setParent(m_dockHost), true);
        (!adoptedTabWidget) && (m_addViewportButton->hide(), true);
        (adoptedTabWidget && adoptedTabBar) &&
            (m_addViewportButton->move(adoptedTabBar->mapTo(
                 adoptedTabWidget,
                 QPoint(
                     adoptedTabBar->tabRect(adoptedTabBar->count() - 1).right() + 4,
                     (adoptedTabBar->height() - m_addViewportButton->height()) / 2))),
             m_addViewportButton->show(), m_addViewportButton->raise(), true);
        const int adoptedTabIndex = adoptedTabWidget ? adoptedTabWidget->indexOf(adoptedDock) : -1;
        const QTabBar::ButtonPosition closeSide = adoptedTabBar
            ? static_cast<QTabBar::ButtonPosition>(
                  adoptedTabBar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, adoptedTabBar))
            : QTabBar::RightSide;
        QWidget* adoptedCloseButton =
            (adoptedTabBar && adoptedTabIndex != -1) ? adoptedTabBar->tabButton(adoptedTabIndex, closeSide) : nullptr;
        adoptedCloseButton &&
            (adoptedTabBar->setTabButton(adoptedTabIndex, closeSide, nullptr), adoptedCloseButton->deleteLater(), true);

        auto* anchorTabWidget = AzQtComponents::DockTabWidget::IsTabbed(m_dockAnchor)
            ? AzQtComponents::DockTabWidget::ParentTabWidget(m_dockAnchor)
            : nullptr;
        const bool anchorPresent = m_dockAnchor->isVisible() || anchorTabWidget != nullptr;
        (!anyViewportDocked && !anchorPresent) &&
            (m_dockAnchor->setTitleBarWidget(new QWidget(m_dockAnchor)),
             m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, m_dockAnchor), m_dockAnchor->show(), true);
        (anyViewportDocked && anchorPresent) &&
            (anchorTabWidget && (anchorTabWidget->removeTab(m_dockAnchor), true),
             m_dockHost->removeDockWidget(m_dockAnchor), true);

        const bool shouldChase = m_viewportUiOverlayWindow && m_activeViewport && m_activeViewport != m_adoptedViewport;

        shouldChase &&
            (m_viewportUiOverlayWindow->setGeometry(QRect(m_activeViewport->mapToGlobal(QPoint(0, 0)), m_activeViewport->size())),
             true);

        (m_viewportUiOverlayWindow && m_viewportUiOverlayWindow->isVisible()) && (m_viewportUiOverlayWindow->raise(), true);

        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ::RPI::ViewportContextPtr defaultContext;
        (m_cameraMirroringEnabled && viewportContextManager) &&
            (defaultContext = viewportContextManager->GetDefaultViewportContext(), true);

        const AZ::Transform mirrored = defaultContext ? defaultContext->GetCameraTransform() : AZ::Transform::CreateIdentity();
        for (HammerWidget* viewport : m_viewports)
        {
            (defaultContext && viewport != m_adoptedViewport) &&
                (AtomToolsFramework::ModularViewportCameraControllerRequestBus::Event(
                     viewport->GetViewportId(),
                     &AtomToolsFramework::ModularViewportCameraControllerRequests::StartTrackingTransform, mirrored),
                 true);
        }
    }

    bool HammerViewportLayoutWidget::eventFilter(QObject* watched, QEvent* event)
    {
        AZ_Assert(watched && event, "eventFilter invoked with a null object or event");
        const bool isOverlayMoveOrResize = watched == m_viewportUiOverlayWindow && m_activeViewport &&
            m_activeViewport != m_adoptedViewport && (event->type() == QEvent::Move || event->type() == QEvent::Resize);

        isOverlayMoveOrResize &&
            (static_cast<QWidget*>(watched)->setGeometry(
                 QRect(m_activeViewport->mapToGlobal(QPoint(0, 0)), m_activeViewport->size())),
             static_cast<QWidget*>(watched)->raise(), true);

        return QWidget::eventFilter(watched, event);
    }

    void HammerViewportLayoutWidget::SetViewFromEntityPerspective(const AZ::EntityId& entityId)
    {
        AZ_Assert(!m_viewports.empty(), "SetViewFromEntityPerspective called with no viewports");
        AZ_Assert(
            !m_adoptedViewport || AZStd::find(m_viewports.begin(), m_viewports.end(), m_adoptedViewport) != m_viewports.end(),
            "The adopted viewport is not tracked in the viewport list");
        (entityId.IsValid() && m_adoptedViewport) && (ActivateViewport(m_adoptedViewport), true);
    }

    AZ::EntityId HammerViewportLayoutWidget::GetCurrentViewEntityId()
    {
        AZ_Assert(m_activeViewport, "GetCurrentViewEntityId called before any viewport was activated");
        return AZ::EntityId();
    }

    bool HammerViewportLayoutWidget::GetActiveCameraPosition(AZ::Vector3& cameraPos)
    {
        AZ_Assert(m_activeViewport, "GetActiveCameraPosition called before any viewport was activated");
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerViewportLayoutWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");
        AZ::RPI::ViewportContextPtr viewportContext;
        (m_activeViewport && viewportContextManager) &&
            (viewportContext = viewportContextManager->GetViewportContextById(m_activeViewport->GetViewportId()), true);
        viewportContext && (cameraPos = viewportContext->GetCameraTransform().GetTranslation(), true);
        return viewportContext != nullptr;
    }

    AZStd::optional<AZ::Transform> HammerViewportLayoutWidget::GetActiveCameraTransform()
    {
        AZ_Assert(m_activeViewport, "GetActiveCameraTransform called before any viewport was activated");
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerViewportLayoutWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");
        AZ::RPI::ViewportContextPtr viewportContext;
        (m_activeViewport && viewportContextManager) &&
            (viewportContext = viewportContextManager->GetViewportContextById(m_activeViewport->GetViewportId()), true);
        return viewportContext ? AZStd::optional<AZ::Transform>(viewportContext->GetCameraTransform()) : AZStd::nullopt;
    }

    AZStd::optional<float> HammerViewportLayoutWidget::GetCameraFoV()
    {
        const float fovRadians = SandboxEditor::CameraDefaultFovRadians();
        AZ_Assert(fovRadians > 0.0f, "CameraDefaultFovRadians returned a non-positive field of view");
        return fovRadians;
    }

    bool HammerViewportLayoutWidget::GetActiveCameraState(AzFramework::CameraState& cameraState)
    {
        AZ_Assert(m_activeViewport, "GetActiveCameraState called before any viewport was activated");
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerViewportLayoutWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");
        AZ::RPI::ViewportContextPtr viewportContext;
        (m_activeViewport && viewportContextManager) &&
            (viewportContext = viewportContextManager->GetViewportContextById(m_activeViewport->GetViewportId()), true);
        viewportContext &&
            (cameraState = AzFramework::CreateDefaultCamera(viewportContext->GetCameraTransform(), AzFramework::ScreenSize(1, 1)),
             true);
        return viewportContext != nullptr;
    }
} // namespace Hammer
