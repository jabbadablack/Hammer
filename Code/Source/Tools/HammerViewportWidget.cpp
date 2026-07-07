#include "HammerViewportWidget.h"
#include "HammerWidget.h"

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraControllerRequestBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/std/algorithm.h>
#include <AzFramework/Viewport/CameraState.h>
#include <AzQtComponents/Components/DockTabWidget.h>
#include <AzQtComponents/Components/FancyDocking.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzQtComponents/Components/Widgets/ToolBar.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInternalInterface.h>
#include <AzToolsFramework/ActionManager/Menu/MenuManagerInternalInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorMenuIdentifiers.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>
#include <Editor/EditorViewportSettings.h>

#include <QAbstractButton>
#include <QAction>
#include <QEvent>
#include <QFile>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

namespace Hammer
{
    QToolBar* HammerViewportWidget::BuildViewportToolBar(size_t viewportIndex)
    {
        AZ_Assert(viewportIndex < m_viewports.size(), "BuildViewportToolBar called with an out-of-range viewport index");
        auto* actionManagerInternal = AZ::Interface<AzToolsFramework::ActionManagerInternalInterface>::Get();
        auto* menuManagerInternal = AZ::Interface<AzToolsFramework::MenuManagerInternalInterface>::Get();
        QAction* readinessProbe = (actionManagerInternal && menuManagerInternal)
            ? actionManagerInternal->GetAction("o3de.action.viewport.menuIcon")
            : nullptr;
        if (!readinessProbe)
        {
            return nullptr;
        }

        auto* toolBar = new QToolBar(this);
        toolBar->setObjectName(QStringLiteral("HammerViewportToolBar"));
        toolBar->setMovable(false);
        toolBar->setIconSize(QSize(16, 16));
        toolBar->setStyleSheet(QStringLiteral("QToolBar { background: transparent; border: none; padding: 0px; }"));
        toolBar->hide();

        QAction* focusUpAction = actionManagerInternal->GetAction("o3de.action.prefabs.focusUpOneLevel");
        AZ_Error(
            "HammerViewportWidget", focusUpAction,
            "Could not find the prefab focus-up action for the Hammer viewport toolbar");
        focusUpAction && (toolBar->addAction(focusUpAction), true);

        QWidget* focusPath = actionManagerInternal->GenerateWidgetFromWidgetAction("o3de.widgetAction.prefab.focusPath");
        AZ_Error(
            "HammerViewportWidget", focusPath,
            "Could not generate the prefab focus path widget for the Hammer viewport toolbar");
        focusPath &&
            (toolBar->addWidget(focusPath)->setObjectName(QStringLiteral("o3de.widgetAction.prefab.focusPath")), true);

        toolBar->addSeparator();

        QWidget* prefabEditMode =
            actionManagerInternal->GenerateWidgetFromWidgetAction("o3de.widgetAction.prefab.editVisualMode");
        AZ_Error(
            "HammerViewportWidget", prefabEditMode,
            "Could not generate the prefab edit mode widget for the Hammer viewport toolbar");
        prefabEditMode &&
            (toolBar->addWidget(prefabEditMode)->setObjectName(QStringLiteral("o3de.widgetAction.prefab.editVisualMode")),
             true);

        toolBar->addSeparator();

        auto* viewModeButton = new QToolButton(toolBar);
        viewModeButton->setIcon(QIcon(QStringLiteral(":/Hammer/toolbar_icon.svg")));
        viewModeButton->setToolTip(QObject::tr("View Mode"));
        viewModeButton->setPopupMode(QToolButton::InstantPopup);
        viewModeButton->setAutoRaise(true);

        QMenu* viewModeMenu = new QMenu(QObject::tr("View Mode"), viewModeButton);
        QAction* normalAction = viewModeMenu->addAction(QObject::tr("Normal"));
        QAction* wireframeAction = viewModeMenu->addAction(QObject::tr("Wireframe"));
        QAction* overdrawAction = viewModeMenu->addAction(QObject::tr("Quad Overdraw"));
        for (QAction* modeAction : { normalAction, wireframeAction, overdrawAction })
        {
            modeAction->setCheckable(true);
            connect(
                modeAction, &QAction::toggled, this,
                [this, viewportIndex, normalAction, wireframeAction, overdrawAction]
                {
                    m_viewports[viewportIndex]->SetViewModes(HammerViewModes{
                        normalAction->isChecked(), wireframeAction->isChecked(), overdrawAction->isChecked() });
                });
        }
        normalAction->setChecked(true);

        viewModeMenu->addSeparator();
        QAction* mirrorAction = viewModeMenu->addAction(QObject::tr("Mirror Main Camera"));
        mirrorAction->setCheckable(true);
        connect(
            mirrorAction, &QAction::toggled, this,
            [this](bool checked)
            {
                SetCameraMirroringEnabled(checked);
            });

        connect(
            viewModeMenu, &QMenu::aboutToShow, this,
            [this, viewportIndex, normalAction, wireframeAction, overdrawAction, mirrorAction]
            {
                const HammerViewModes viewModes = m_viewports[viewportIndex]->GetViewModes();
                const QSignalBlocker normalBlocker(normalAction);
                const QSignalBlocker wireframeBlocker(wireframeAction);
                const QSignalBlocker overdrawBlocker(overdrawAction);
                const QSignalBlocker mirrorBlocker(mirrorAction);
                normalAction->setChecked(viewModes.m_normal);
                wireframeAction->setChecked(viewModes.m_wireframe);
                overdrawAction->setChecked(viewModes.m_overdraw);
                mirrorAction->setChecked(m_cameraMirroringEnabled);
            });

        viewModeButton->setMenu(viewModeMenu);
        toolBar->addWidget(viewModeButton);

        const struct
        {
            const char* m_actionId;
            AZStd::string_view m_menuId;
        } menuButtons[] = {
            { "o3de.action.view.goToPosition", EditorIdentifiers::ViewportCameraMenuIdentifier },
            { "o3de.action.viewport.info.toggle", EditorIdentifiers::ViewportDebugInfoMenuIdentifier },
            { "o3de.action.view.showHelpers", EditorIdentifiers::ViewportHelpersMenuIdentifier },
            { "o3de.action.viewport.resizeIcon", EditorIdentifiers::ViewportSizeMenuIdentifier },
            { "o3de.action.viewport.menuIcon", EditorIdentifiers::ViewportOptionsMenuIdentifier },
        };
        for (const auto& menuButton : menuButtons)
        {
            QAction* action = actionManagerInternal->GetAction(menuButton.m_actionId);
            QMenu* menu = menuManagerInternal->GetMenu(AZStd::string(menuButton.m_menuId));
            AZ_Error(
                "HammerViewportWidget", action && menu,
                "Viewport toolbar entry '%s' is missing its action or menu", menuButton.m_actionId);
            QToolButton* button = (action && menu) ? new QToolButton(toolBar) : nullptr;
            button &&
                (button->setPopupMode(QToolButton::MenuButtonPopup), button->setAutoRaise(true), button->setMenu(menu),
                 button->setDefaultAction(action), toolBar->addWidget(button), true);
        }

        QToolButton* expander = AzQtComponents::ToolBar::getToolBarExpansionButton(toolBar);
        AZ_Assert(expander, "The viewport toolbar has no expansion button to collapse into");
        expander && (expander->installEventFilter(this), true);

        return toolBar;
    }

    HammerViewportWidget::HammerViewportWidget(QWidget* parent)
        : QWidget(parent)
    {
        AZ_Assert(
            !HammerEditorActiveViewportRequestBus::HasHandlers(),
            "HammerViewportWidget constructed while another HammerEditorActiveViewportRequestBus handler exists");

        hide();

        const QList<QWidget*> topLevelChildren = parent ? parent->window()->findChildren<QWidget*>() : QList<QWidget*>();
        const auto fancyDockingIt = AZStd::find_if(
            topLevelChildren.begin(), topLevelChildren.end(),
            [](QWidget* child)
            {
                return qstrcmp(child->metaObject()->className(), "AzQtComponents::FancyDocking") == 0;
            });
        m_fancyDocking =
            fancyDockingIt != topLevelChildren.end() ? static_cast<AzQtComponents::FancyDocking*>(*fancyDockingIt) : nullptr;
        AZ_Error(
            "HammerViewportWidget", m_fancyDocking,
            "Could not find the editor's FancyDocking manager; viewports will not be dockable");

        m_dockHost = m_fancyDocking ? qobject_cast<QMainWindow*>(m_fancyDocking->parentWidget()) : qobject_cast<QMainWindow*>(parent);
        AZ_Assert(m_dockHost, "HammerViewportWidget could not resolve the editor's view pane host QMainWindow");

        QWidget* oldCentralWidget = m_dockHost ? m_dockHost->takeCentralWidget() : nullptr;
        oldCentralWidget && (oldCentralWidget->hide(), true);

        for (int i = 0; i < MaxViewportCount; ++i)
        {
            auto* dock = new AzQtComponents::StyledDockWidget(QStringLiteral("Perspective %1").arg(i + 1), this);
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

        AZ_Assert(m_viewports.size() == MaxViewportCount, "HammerViewportWidget failed to create all %d viewports", MaxViewportCount);
        m_viewportToolBars.resize(m_viewports.size());

        m_dockAnchor = new AzQtComponents::StyledDockWidget(QStringLiteral("Viewport Dock Area"), this);
        m_dockAnchor->setObjectName(QStringLiteral("HammerViewportDockAnchor"));
        m_dockAnchor->setFeatures(QDockWidget::NoDockWidgetFeatures);
        m_dockAnchor->setTitleBarWidget(new QWidget(m_dockAnchor));
        m_dockAnchor->setWidget(new QWidget(m_dockAnchor));
        m_dockAnchor->hide();

        m_addViewportAction = new QAction(QIcon(QStringLiteral(":/Hammer/add-view.svg")), QObject::tr("Add Hammer Viewport"), this);
        m_addViewportAction->setObjectName(QStringLiteral("HammerAddViewportButton"));
        AZ_Warning(
            "HammerViewportWidget", !m_addViewportAction->icon().isNull(),
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
                (unused && adoptedDock && m_fancyDocking) &&
                    (m_fancyDocking->tabifyDockWidget(
                         adoptedDock, qobject_cast<QDockWidget*>(unused->parentWidget()), m_dockHost),
                     unused->show(), true);
            });

        auto* firstDock = qobject_cast<QDockWidget*>(m_viewports[0]->parentWidget());
        AZ_Assert(firstDock, "The startup viewport is not hosted in a QDockWidget");
        m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, firstDock);
        m_dockHost->resizeDocks({ firstDock }, { m_dockHost->width() * 2 / 3 }, Qt::Horizontal);
        firstDock->show();
        m_viewports[0]->SetRenderTickEnabled(true);
        ActivateViewport(m_viewports[0]);

        HammerEditorActiveViewportRequestBus::Handler::BusConnect();
        HammerViewportRequestBus::Handler::BusConnect();
        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusConnect();
        Camera::EditorCameraRequestBus::Handler::BusConnect();
    }

    HammerViewportWidget::~HammerViewportWidget()
    {
        AZ_Assert(
            HammerEditorActiveViewportRequestBus::Handler::BusIsConnected(),
            "HammerViewportWidget destroyed while its active-viewport bus was already disconnected");
        AZ_Assert(
            HammerViewportRequestBus::Handler::BusIsConnected(),
            "HammerViewportWidget destroyed while its viewport-request bus was already disconnected");

        Camera::EditorCameraRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusDisconnect();
        HammerViewportRequestBus::Handler::BusDisconnect();
        HammerEditorActiveViewportRequestBus::Handler::BusDisconnect();

        for (QPointer<QToolBar>& toolBar : m_viewportToolBars)
        {
            toolBar && (delete toolBar.data(), true);
        }
        m_viewportToolBars.clear();

        for (HammerWidget* viewport : m_viewports)
        {
            QWidget* dock = qobject_cast<QDockWidget*>(viewport->parentWidget());
            delete (dock ? dock : static_cast<QWidget*>(viewport));
        }
        m_viewports.clear();

        delete m_dockAnchor;
        m_dockAnchor = nullptr;
        delete m_addViewportButton;
    }

    void HammerViewportWidget::SetActiveViewportId(AzFramework::ViewportId viewportId)
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

    AzFramework::ViewportId HammerViewportWidget::GetActiveViewportId() const
    {
        return m_activeViewportId;
    }

    void HammerViewportWidget::OnStartGameModeRequest()
    {
        AZ_Assert(m_adoptedViewport, "Game mode was requested before the real viewport was adopted");
        AZ_Assert(!m_preGameModeActiveViewport, "Game mode started while a previous pre-game-mode viewport is still pending");
        m_adoptedViewport && (m_preGameModeActiveViewport = m_activeViewport, ActivateViewport(m_adoptedViewport), true);
        auto* adoptedDock = m_adoptedViewport ? qobject_cast<QDockWidget*>(m_adoptedViewport->parentWidget()) : nullptr;
        auto* adoptedTabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(adoptedDock);
        adoptedTabWidget && (adoptedTabWidget->setCurrentWidget(adoptedDock), true);
        for (HammerWidget* viewport : m_viewports)
        {
            viewport->SetGameModeSuppressed(true);
        }
    }

    void HammerViewportWidget::OnStopGameModeRequest()
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
        auto* previousDock =
            m_preGameModeActiveViewport ? qobject_cast<QDockWidget*>(m_preGameModeActiveViewport->parentWidget()) : nullptr;
        auto* previousTabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(previousDock);
        previousTabWidget && (previousTabWidget->setCurrentWidget(previousDock), true);
        m_preGameModeActiveViewport && (ActivateViewport(m_preGameModeActiveViewport), true);
        m_preGameModeActiveViewport = nullptr;
    }

    void HammerViewportWidget::ActivateViewport(HammerWidget* viewport)
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
    }

    void HammerViewportWidget::AdoptRealPerspectiveViewport(QWidget& realViewport)
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
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        dock->setWidget(adopted);
        adopted->show();
        realViewport.show();
        adopted->SetRenderTickEnabled(true);

        ActivateViewport(adopted);

        delete placeholder;

        ResolveViewportUiOverlayWindow();
    }

    void HammerViewportWidget::ResolveViewportUiOverlayWindow()
    {
        AZ_Assert(m_adoptedViewport, "ResolveViewportUiOverlayWindow called before the real viewport was adopted");
        AZ_Assert(!m_overlaySyncTimer, "ResolveViewportUiOverlayWindow should only start the sync timer once");

        m_viewportUiOverlayWindow = m_adoptedViewport->findChild<QWidget*>(QStringLiteral("ViewportUiWindow"));
        AZ_Warning(
            "HammerViewportWidget", m_viewportUiOverlayWindow,
            "Could not find the adopted viewport's ViewportUiWindow overlay; viewport UI will not follow the active viewport");
        m_viewportUiOverlayWindow && (m_viewportUiOverlayWindow->installEventFilter(this), true);

        m_overlaySyncTimer = new QTimer(this);
        connect(m_overlaySyncTimer, &QTimer::timeout, this, &HammerViewportWidget::SyncViewportUiOverlay);
        m_overlaySyncTimer->start(16);
    }

    void HammerViewportWidget::SetCameraMirroringEnabled(bool enabled)
    {
        AZ_Assert(!enabled || m_overlaySyncTimer, "Camera mirroring enabled before the overlay sync timer exists; mirroring is driven by that timer");
        m_cameraMirroringEnabled = enabled;
    }

    void HammerViewportWidget::SetActiveViewportViewModes(bool normal, bool wireframe, bool overdraw)
    {
        AZ_Assert(m_activeViewport, "SetActiveViewportViewModes called before any viewport was activated");
        m_activeViewport && (m_activeViewport->SetViewModes(HammerViewModes{ normal, wireframe, overdraw }), true);
    }

    void HammerViewportWidget::SyncViewportUiOverlay()
    {
        AZ_Assert(m_overlaySyncTimer, "SyncViewportUiOverlay ticked without its timer");
        AZ_Assert(m_adoptedViewport, "SyncViewportUiOverlay ticked before the real viewport was adopted");
        AZ_Assert(m_dockAnchor, "SyncViewportUiOverlay ticked before the dock anchor was constructed");

        auto* adoptedDock = qobject_cast<QDockWidget*>(m_adoptedViewport->parentWidget());
        AZ_Assert(adoptedDock, "The adopted viewport is not hosted in a QDockWidget");

        for (HammerWidget* viewport : m_viewports)
        {
            viewport->SetRenderTickEnabled(viewport->isVisible());
        }

        bool anyDockDocked = false;
        const QList<QDockWidget*> hostDocks = m_dockHost->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QDockWidget* hostDock : hostDocks)
        {
            anyDockDocked = anyDockDocked ||
                (hostDock != m_dockAnchor && hostDock->isVisible() && !hostDock->isFloating() &&
                 hostDock->window() == m_dockHost->window());
        }

        (adoptedDock && !adoptedDock->isVisible() && !AzQtComponents::DockTabWidget::IsTabbed(adoptedDock) &&
         adoptedDock->window() == m_dockHost->window()) &&
            (m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, adoptedDock), adoptedDock->show(), anyDockDocked = true, true);

        const bool adoptedDockedInHost = adoptedDock && adoptedDock->isVisible() && !adoptedDock->isFloating() &&
            adoptedDock->window() == m_dockHost->window();
        auto* adoptedTabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(adoptedDock);
        (m_fancyDocking && adoptedDockedInHost && !adoptedTabWidget) &&
            (m_fancyDocking->createTabWidget(m_dockHost, adoptedDock), true);
        adoptedTabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(adoptedDock);

        !m_addViewportButton &&
            (m_addViewportButton = new QToolButton(m_dockHost),
             m_addViewportButton->setObjectName(QStringLiteral("HammerAddViewportButton")),
             m_addViewportButton->setDefaultAction(m_addViewportAction), m_addViewportButton->setAutoRaise(true),
             m_addViewportButton->setStyleSheet(
                 []
                 {
                     QFile styleSheetFile(QStringLiteral(":/Hammer/Hammer.qss"));
                     return styleSheetFile.open(QFile::ReadOnly) ? QString::fromUtf8(styleSheetFile.readAll()) : QString();
                 }()),
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

        AZ_Assert(
            m_viewportToolBars.size() == m_viewports.size(),
            "The viewport toolbar list is out of sync with the viewport list");
        for (size_t i = 0; i < m_viewports.size(); ++i)
        {
            auto* dock = qobject_cast<QDockWidget*>(m_viewports[i]->parentWidget());
            auto* tabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(dock);
            QTabBar* tabBar = tabWidget ? tabWidget->tabBar() : nullptr;
            QWidget* titleBar = (dock && !tabWidget) ? dock->titleBarWidget() : nullptr;

            QPointer<QToolBar>& toolBar = m_viewportToolBars[i];
            (!toolBar) && (toolBar = BuildViewportToolBar(i), true);

            const bool currentInTab = tabWidget && tabBar && tabWidget->currentWidget() == dock;
            const bool soloWithTitle = titleBar && dock->isVisible();
            QWidget* host = currentInTab ? static_cast<QWidget*>(tabWidget) : (soloWithTitle ? static_cast<QWidget*>(dock) : nullptr);

            QTabBar* titleTabBar = titleBar ? titleBar->findChild<QTabBar*>() : nullptr;
            titleTabBar = (titleTabBar && titleTabBar->isVisible() && titleTabBar->count() > 0) ? titleTabBar : nullptr;
            const int occupiedLeft = currentInTab
                ? tabBar->mapTo(tabWidget, QPoint(tabBar->tabRect(tabBar->count() - 1).right(), 0)).x() + 4 +
                    ((m_addViewportButton && m_addViewportButton->parentWidget() == tabWidget)
                         ? m_addViewportButton->width() + 8
                         : 0)
                : (soloWithTitle
                       ? (titleTabBar
                              ? titleTabBar->mapTo(dock, QPoint(titleTabBar->tabRect(titleTabBar->count() - 1).right(), 0)).x() + 4
                              : titleBar->fontMetrics().horizontalAdvance(dock->windowTitle()) + 24)
                       : 0);

            int titleButtonsLeft = host ? host->width() : 0;
            const QList<QAbstractButton*> titleButtons =
                titleBar ? titleBar->findChildren<QAbstractButton*>() : QList<QAbstractButton*>();
            for (QAbstractButton* button : titleButtons)
            {
                const int buttonLeft = button->mapTo(dock, QPoint(0, 0)).x();
                titleButtonsLeft =
                    (button->isVisible() && buttonLeft < titleButtonsLeft) ? buttonLeft : titleButtonsLeft;
            }
            const int clearance = currentInTab ? 4 : (soloWithTitle ? host->width() - titleButtonsLeft + 4 : 0);

            (toolBar && !host) && (toolBar->hide(), true);
            (toolBar && host && toolBar->parentWidget() != host) && (toolBar->setParent(host), true);
            (toolBar && host) &&
                (toolBar->resize(
                     AZStd::clamp(host->width() - occupiedLeft - clearance, 28, toolBar->sizeHint().width()),
                     toolBar->sizeHint().height()),
                 toolBar->move(
                     host->width() - toolBar->width() - clearance,
                     currentInTab
                         ? tabBar->mapTo(tabWidget, QPoint(0, (tabBar->height() - toolBar->height()) / 2)).y()
                         : titleBar->geometry().y() + (titleBar->height() - toolBar->height()) / 2),
                 toolBar->show(), toolBar->raise(), true);
        }

        auto* anchorTabWidget = AzQtComponents::DockTabWidget::IsTabbed(m_dockAnchor)
            ? AzQtComponents::DockTabWidget::ParentTabWidget(m_dockAnchor)
            : nullptr;
        const bool anchorPresent = m_dockAnchor->isVisible() || anchorTabWidget != nullptr;
        (!anyDockDocked && !anchorPresent) &&
            (m_dockAnchor->setTitleBarWidget(new QWidget(m_dockAnchor)),
             m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, m_dockAnchor), m_dockAnchor->show(), true);
        (anyDockDocked && anchorPresent) &&
            (anchorTabWidget && (anchorTabWidget->removeTab(m_dockAnchor), true),
             m_dockHost->removeDockWidget(m_dockAnchor), m_dockAnchor->setParent(this), m_dockAnchor->hide(), true);

        const bool shouldChase = m_viewportUiOverlayWindow && m_activeViewport && m_activeViewport != m_adoptedViewport;

        shouldChase &&
            (m_viewportUiOverlayWindow->setGeometry(QRect(m_activeViewport->mapToGlobal(QPoint(0, 0)), m_activeViewport->size())),
             true);

        QWidget* activeViewportWindow = m_activeViewport ? m_activeViewport->window() : nullptr;
        (m_viewportUiOverlayWindow && m_viewportUiOverlayWindow->isVisible() && activeViewportWindow &&
         activeViewportWindow->isActiveWindow()) &&
            (m_viewportUiOverlayWindow->raise(), true);

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

    bool HammerViewportWidget::eventFilter(QObject* watched, QEvent* event)
    {
        AZ_Assert(watched && event, "eventFilter invoked with a null object or event");
        auto* expander = qobject_cast<QToolButton*>(watched);
        auto* expanderToolBar = expander ? qobject_cast<QToolBar*>(expander->parentWidget()) : nullptr;
        const bool isToolBarExpanderClick = expanderToolBar &&
            expanderToolBar->objectName() == QLatin1String("HammerViewportToolBar") &&
            (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
             event->type() == QEvent::MouseButtonDblClick);
        if (isToolBarExpanderClick)
        {
            auto* actionManagerInternal = AZ::Interface<AzToolsFramework::ActionManagerInternalInterface>::Get();
            AZ_Assert(actionManagerInternal, "Toolbar overflow menu requested without the ActionManager");
            if (event->type() != QEvent::MouseButtonPress || !actionManagerInternal)
            {
                return true;
            }

            QMenu menu(expander);
            for (QAction* action : expanderToolBar->actions())
            {
                QWidget* itemWidget = expanderToolBar->widgetForAction(action);
                if (!itemWidget || itemWidget->isVisible())
                {
                    continue;
                }
                auto* widgetAction = qobject_cast<QWidgetAction*>(action);
                auto* menuButton = widgetAction ? qobject_cast<QToolButton*>(widgetAction->defaultWidget()) : nullptr;
                QWidget* generated = (widgetAction && !menuButton)
                    ? actionManagerInternal->GenerateWidgetFromWidgetAction(widgetAction->objectName().toUtf8().constData())
                    : nullptr;
                QWidgetAction* generatedAction = generated ? new QWidgetAction(&menu) : nullptr;
                (menuButton && menuButton->menu()) && (menu.addMenu(menuButton->menu()), true);
                generatedAction && (generatedAction->setDefaultWidget(generated), menu.addAction(generatedAction), true);
                (!widgetAction && action->isSeparator()) && (menu.addSeparator(), true);
                (!widgetAction && !action->isSeparator()) && (menu.addAction(action), true);
            }
            menu.exec(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        }

        const bool isOverlayMoveOrResize = watched == m_viewportUiOverlayWindow && m_activeViewport &&
            m_activeViewport != m_adoptedViewport && (event->type() == QEvent::Move || event->type() == QEvent::Resize);

        isOverlayMoveOrResize &&
            (static_cast<QWidget*>(watched)->setGeometry(
                 QRect(m_activeViewport->mapToGlobal(QPoint(0, 0)), m_activeViewport->size())),
             true);

        return QWidget::eventFilter(watched, event);
    }

    void HammerViewportWidget::SetViewFromEntityPerspective(const AZ::EntityId& entityId)
    {
        AZ_Assert(!m_viewports.empty(), "SetViewFromEntityPerspective called with no viewports");
        AZ_Assert(
            !m_adoptedViewport || AZStd::find(m_viewports.begin(), m_viewports.end(), m_adoptedViewport) != m_viewports.end(),
            "The adopted viewport is not tracked in the viewport list");
        (entityId.IsValid() && m_adoptedViewport) && (ActivateViewport(m_adoptedViewport), true);
    }

    AZ::EntityId HammerViewportWidget::GetCurrentViewEntityId()
    {
        AZ_Assert(m_activeViewport, "GetCurrentViewEntityId called before any viewport was activated");
        return AZ::EntityId();
    }

    bool HammerViewportWidget::GetActiveCameraPosition(AZ::Vector3& cameraPos)
    {
        AZ_Assert(m_activeViewport, "GetActiveCameraPosition called before any viewport was activated");
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerViewportWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");
        AZ::RPI::ViewportContextPtr viewportContext;
        (m_activeViewport && viewportContextManager) &&
            (viewportContext = viewportContextManager->GetViewportContextById(m_activeViewport->GetViewportId()), true);
        viewportContext && (cameraPos = viewportContext->GetCameraTransform().GetTranslation(), true);
        return viewportContext != nullptr;
    }

    AZStd::optional<AZ::Transform> HammerViewportWidget::GetActiveCameraTransform()
    {
        AZ_Assert(m_activeViewport, "GetActiveCameraTransform called before any viewport was activated");
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerViewportWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");
        AZ::RPI::ViewportContextPtr viewportContext;
        (m_activeViewport && viewportContextManager) &&
            (viewportContext = viewportContextManager->GetViewportContextById(m_activeViewport->GetViewportId()), true);
        return viewportContext ? AZStd::optional<AZ::Transform>(viewportContext->GetCameraTransform()) : AZStd::nullopt;
    }

    AZStd::optional<float> HammerViewportWidget::GetCameraFoV()
    {
        const float fovRadians = SandboxEditor::CameraDefaultFovRadians();
        AZ_Assert(fovRadians > 0.0f, "CameraDefaultFovRadians returned a non-positive field of view");
        return fovRadians;
    }

    bool HammerViewportWidget::GetActiveCameraState(AzFramework::CameraState& cameraState)
    {
        AZ_Assert(m_activeViewport, "GetActiveCameraState called before any viewport was activated");
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerViewportWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");
        AZ::RPI::ViewportContextPtr viewportContext;
        (m_activeViewport && viewportContextManager) &&
            (viewportContext = viewportContextManager->GetViewportContextById(m_activeViewport->GetViewportId()), true);
        viewportContext &&
            (cameraState = AzFramework::CreateDefaultCamera(viewportContext->GetCameraTransform(), AzFramework::ScreenSize(1, 1)),
             true);
        return viewportContext != nullptr;
    }
} // namespace Hammer
