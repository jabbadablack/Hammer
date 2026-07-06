#include "HammerViewportLayoutWidget.h"
#include "HammerWidget.h"

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraControllerRequestBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Viewport/CameraState.h>
#include <AzQtComponents/Components/DockMainWindow.h>
#include <AzQtComponents/Components/DockTabWidget.h>
#include <AzQtComponents/Components/FancyDocking.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>
#include <Editor/EditorViewportSettings.h>

#include <QEvent>
#include <QTimer>
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

        auto* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);

        m_dockHost = new AzQtComponents::DockMainWindow;
        m_dockHost->setDockOptions(QMainWindow::GroupedDragging | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
        outerLayout->addWidget(m_dockHost);

        m_fancyDocking = new AzQtComponents::FancyDocking(m_dockHost, "hammerviewportdocking");

        for (int i = 0; i < MaxViewportCount; ++i)
        {
            auto* dock = new AzQtComponents::StyledDockWidget(QStringLiteral("Viewport %1").arg(i + 1), m_dockHost);
            dock->setObjectName(QStringLiteral("HammerViewportDock%1").arg(i + 1));
            dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

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

        SetViewportCount(1);

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

    void HammerViewportLayoutWidget::ReconcileGridSlots(int shownCount, int columns)
    {
        AZ_Assert(
            shownCount >= MinViewportCount && shownCount <= MaxViewportCount,
            "ReconcileGridSlots given an out-of-range shownCount %d", shownCount);
        AZ_Assert(columns == 1 || columns == 2, "ReconcileGridSlots given an unexpected column count %d", columns);
        AZ_Assert(m_fancyDocking, "ReconcileGridSlots called before the fancy docking manager was constructed");
        AZ_Assert(m_dockAnchor, "ReconcileGridSlots called before the dock anchor was constructed");

        m_adoptedViewportHiddenBehindMaximize = false;

        auto* anchorTabWidget = AzQtComponents::DockTabWidget::IsTabbed(m_dockAnchor)
            ? AzQtComponents::DockTabWidget::ParentTabWidget(m_dockAnchor)
            : nullptr;
        anchorTabWidget && (anchorTabWidget->removeTab(m_dockAnchor), true);
        m_dockHost->removeDockWidget(m_dockAnchor);

        for (HammerWidget* viewport : m_viewports)
        {
            auto* dock = qobject_cast<QDockWidget*>(viewport->parentWidget());
            AZ_Assert(dock, "ReconcileGridSlots found a viewport that is not hosted in a QDockWidget");
            auto* tabWidget =
                AzQtComponents::DockTabWidget::IsTabbed(dock) ? AzQtComponents::DockTabWidget::ParentTabWidget(dock) : nullptr;
            tabWidget && (tabWidget->removeTab(dock), true);
        }

        auto* firstDock = qobject_cast<QDockWidget*>(m_viewports[0]->parentWidget());
        AZ_Assert(firstDock, "ReconcileGridSlots expects slot 0's viewport to be hosted in a QDockWidget");
        firstDock->setParent(m_dockHost);
        m_dockHost->removeDockWidget(firstDock);
        m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, firstDock);
        firstDock->show();
        m_viewports[0]->SetRenderTickEnabled(true);

        for (int i = columns; i < shownCount; i += columns)
        {
            auto* anchorDock = qobject_cast<QDockWidget*>(m_viewports[i - columns]->parentWidget());
            auto* dock = qobject_cast<QDockWidget*>(m_viewports[i]->parentWidget());
            AZ_Assert(anchorDock && dock, "ReconcileGridSlots found a column viewport without a QDockWidget host");
            dock->setParent(m_dockHost);
            m_fancyDocking->splitDockWidget(m_dockHost, anchorDock, dock, Qt::Vertical);
            dock->show();
            m_viewports[i]->SetRenderTickEnabled(true);
        }

        for (int i = 1; i < shownCount; ++i)
        {
            auto* anchorDock = qobject_cast<QDockWidget*>(m_viewports[i - 1]->parentWidget());
            auto* dock = qobject_cast<QDockWidget*>(m_viewports[i]->parentWidget());
            AZ_Assert(anchorDock && dock, "ReconcileGridSlots found a row viewport without a QDockWidget host");
            const bool sameRow = (i % columns) != 0;
            sameRow &&
                (dock->setParent(m_dockHost), m_fancyDocking->splitDockWidget(m_dockHost, anchorDock, dock, Qt::Horizontal),
                 dock->show(), m_viewports[i]->SetRenderTickEnabled(true), true);
        }

        for (int i = shownCount; i < MaxViewportCount; ++i)
        {
            HammerWidget* viewport = m_viewports[i];
            auto* dock = qobject_cast<QDockWidget*>(viewport->parentWidget());
            AZ_Assert(dock, "ReconcileGridSlots found a hidden viewport without a QDockWidget host");
            const bool hideAdoptedBehindMaximize = viewport == m_adoptedViewport && m_maximizedFromIndex != -1;
            dock->setParent(m_dockHost);
            m_dockHost->removeDockWidget(dock);
            (!hideAdoptedBehindMaximize) && (viewport->SetRenderTickEnabled(false), true);
            hideAdoptedBehindMaximize &&
                (dock->setGeometry(m_dockHost->rect()), dock->lower(), dock->show(), viewport->SetRenderTickEnabled(true),
                 m_adoptedViewportHiddenBehindMaximize = true, true);
        }
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        AZ_Assert(m_dockHost, "SetViewportCount called before the dock host was constructed");
        AZ_Assert(m_viewports.size() == MaxViewportCount, "SetViewportCount expects all %d viewports to already exist", MaxViewportCount);

        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);

        (count != 1) && (RestoreMaximizeSwap(), true);

        const int columns = 1 + static_cast<int>(count > 1);
        ReconcileGridSlots(count, columns);

        ActivateViewport(m_viewports[0]);

        m_currentViewportCount = count;
        emit ViewportCountChanged(count);
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

        dock->setWindowTitle(QObject::tr("Perspective"));
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
        auto* adoptedDock =
            m_adoptedViewport ? qobject_cast<QDockWidget*>(m_adoptedViewport->parentWidget()) : nullptr;
        (m_adoptedViewportHiddenBehindMaximize && adoptedDock) &&
            (adoptedDock->setGeometry(m_dockHost->rect()), adoptedDock->lower(), true);

        bool anyViewportDocked = false;
        for (HammerWidget* viewport : m_viewports)
        {
            auto* dock = qobject_cast<QDockWidget*>(viewport->parentWidget());
            const bool hiddenBehindMaximize = m_adoptedViewportHiddenBehindMaximize && viewport == m_adoptedViewport;
            anyViewportDocked = anyViewportDocked ||
                (dock && !hiddenBehindMaximize && dock->isVisible() && !dock->isFloating() &&
                 dock->window() == m_dockHost->window());
        }

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

    void HammerViewportLayoutWidget::RestoreMaximizeSwap()
    {
        AZ_Assert(
            m_maximizedFromIndex == -1 || m_maximizedFromIndex < static_cast<int>(m_viewports.size()),
            "RestoreMaximizeSwap has an out-of-range maximized index %d", m_maximizedFromIndex);
        (m_maximizedFromIndex != -1) && (AZStd::swap(m_viewports[0], m_viewports[m_maximizedFromIndex]), true);
        m_maximizedFromIndex = -1;
    }

    void HammerViewportLayoutWidget::MaximizeActiveViewport()
    {
        AZ_Assert(m_maximizedFromIndex == -1, "MaximizeActiveViewport called while already maximized");
        AZ_Assert(m_activeViewport, "MaximizeActiveViewport called before any viewport was ever activated");

        const auto it = AZStd::find(m_viewports.begin(), m_viewports.end(), m_activeViewport);
        AZ_Assert(it != m_viewports.end(), "The active viewport is not tracked in the viewport list");
        const int activeIndex = it != m_viewports.end() ? static_cast<int>(AZStd::distance(m_viewports.begin(), it)) : 0;

        m_preMaximizeViewportCount = m_currentViewportCount;
        m_maximizedFromIndex = activeIndex;
        AZStd::swap(m_viewports[0], m_viewports[activeIndex]);

        SetViewportCount(1);
    }

    void HammerViewportLayoutWidget::RestoreFromMaximize()
    {
        AZ_Assert(m_maximizedFromIndex != -1, "RestoreFromMaximize called while not maximized");
        AZ_Assert(
            m_preMaximizeViewportCount >= MinViewportCount && m_preMaximizeViewportCount <= MaxViewportCount,
            "RestoreFromMaximize has an out-of-range pre-maximize viewport count %d", m_preMaximizeViewportCount);

        const int restoreCount = m_preMaximizeViewportCount;
        RestoreMaximizeSwap();
        SetViewportCount(restoreCount);
    }

    void HammerViewportLayoutWidget::ToggleMaximizeActiveViewport()
    {
        AZ_Assert(m_activeViewport, "ToggleMaximizeActiveViewport called before any viewport was activated");
        using Action = void (HammerViewportLayoutWidget::*)();
        static constexpr AZStd::array<Action, 2> Actions = {
            &HammerViewportLayoutWidget::MaximizeActiveViewport, &HammerViewportLayoutWidget::RestoreFromMaximize
        };
        (this->*Actions[static_cast<size_t>(m_maximizedFromIndex != -1)])();
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
