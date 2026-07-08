#include "HammerViewportWidget.h"

#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraControllerRequestBus.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/algorithm.h>
#include <AzQtComponents/Components/DockTabWidget.h>
#include <AzQtComponents/Components/FancyDocking.h>
#include <AzQtComponents/Components/StyledDockWidget.h>
#include <AzQtComponents/Components/Widgets/ToolBar.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInternalInterface.h>
#include <AzToolsFramework/ActionManager/Menu/MenuManagerInternalInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorMenuIdentifiers.h>
#include <Editor/EditorViewportSettings.h>
#include <Editor/EditorViewportWidget.h>

#include <QAbstractButton>
#include <QAction>
#include <QDockWidget>
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
    QToolBar* ViewportLayout::BuildViewportToolBar(size_t viewportIndex)
    {
        AZ_Assert(viewportIndex < m_slots.size(), "BuildViewportToolBar called with an out-of-range viewport index");
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
        AZ_Error("Hammer", focusUpAction, "Could not find the prefab focus-up action for the viewport toolbar");
        focusUpAction && (toolBar->addAction(focusUpAction), true);

        QWidget* focusPath = actionManagerInternal->GenerateWidgetFromWidgetAction("o3de.widgetAction.prefab.focusPath");
        AZ_Error("Hammer", focusPath, "Could not generate the prefab focus path widget for the viewport toolbar");
        focusPath &&
            (toolBar->addWidget(focusPath)->setObjectName(QStringLiteral("o3de.widgetAction.prefab.focusPath")), true);

        toolBar->addSeparator();

        QWidget* prefabEditMode =
            actionManagerInternal->GenerateWidgetFromWidgetAction("o3de.widgetAction.prefab.editVisualMode");
        AZ_Error("Hammer", prefabEditMode, "Could not generate the prefab edit mode widget for the viewport toolbar");
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
                    m_slots[viewportIndex].m_viewModes =
                        ViewModes{ normalAction->isChecked(), wireframeAction->isChecked(), overdrawAction->isChecked() };
                    ApplyViewModes(m_slots[viewportIndex]);
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
                const ViewModes viewModes = m_slots[viewportIndex].m_viewModes;
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
            AZ_Error("Hammer", action && menu, "Viewport toolbar entry '%s' is missing its action or menu", menuButton.m_actionId);
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

    ViewportLayout::ViewportLayout(QMainWindow* viewPaneHost, EditorViewportWidget& realViewport)
        : QWidget(viewPaneHost)
    {
        AZ_Assert(viewPaneHost, "ViewportLayout constructed without the editor's view pane host");

        hide();

        const QList<QWidget*> topLevelChildren = viewPaneHost->window()->findChildren<QWidget*>();
        const auto fancyDockingIt = AZStd::find_if(
            topLevelChildren.begin(), topLevelChildren.end(),
            [](QWidget* child)
            {
                return qstrcmp(child->metaObject()->className(), "AzQtComponents::FancyDocking") == 0;
            });
        m_fancyDocking =
            fancyDockingIt != topLevelChildren.end() ? static_cast<AzQtComponents::FancyDocking*>(*fancyDockingIt) : nullptr;
        AZ_Error("Hammer", m_fancyDocking, "Could not find the editor's FancyDocking manager; viewports will not be dockable");

        m_dockHost = m_fancyDocking ? qobject_cast<QMainWindow*>(m_fancyDocking->parentWidget()) : viewPaneHost;
        AZ_Assert(m_dockHost, "ViewportLayout could not resolve the editor's view pane host QMainWindow");

        QWidget* oldCentralWidget = m_dockHost ? m_dockHost->takeCentralWidget() : nullptr;
        oldCentralWidget && (oldCentralWidget->hide(), true);

        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Assert(viewportContextManager, "ViewportLayout constructed before the ViewportContextManager exists");

        for (int i = 0; i < MaxViewportCount; ++i)
        {
            auto* dock = new AzQtComponents::StyledDockWidget(
                i == 0 ? QObject::tr("Perspective") : QStringLiteral("Perspective %1").arg(i + 1), this);
            dock->setObjectName(QStringLiteral("HammerViewportDock%1").arg(i + 1));
            dock->setFeatures(
                i == 0 ? QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                       : QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            dock->hide();

            EditorViewportWidget* engineViewport =
                i == 0 ? &realViewport : new EditorViewportWidget(QStringLiteral("Perspective %1").arg(i + 1));
            AZ_Assert(
                i == 0 || !viewportContextManager || !viewportContextManager->GetViewportContextById(i),
                "ViewportContext id %d is already taken; Hammer cannot claim it for a new viewport", i);
            (i != 0) &&
                (engineViewport->resize(256, 256), static_cast<QtViewport*>(engineViewport)->SetViewportId(i),
                 engineViewport->ConnectViewportInteractionRequestBus(), true);

            (i == 0) && (engineViewport->hide(), engineViewport->setParent(nullptr), true);
            engineViewport->setMinimumSize(50, 50);
            dock->setWidget(engineViewport);
            (i == 0) && (engineViewport->show(), true);

            Slot& slot = m_slots[i];
            slot.m_engineViewport = engineViewport;
            slot.m_dock = dock;

            const AzFramework::ViewportId viewportId = engineViewport->GetViewportId();
            AZ::RPI::ViewportContextPtr viewportContext =
                viewportContextManager ? viewportContextManager->GetViewportContextById(viewportId) : nullptr;
            AZ_Error("Hammer", viewportContext, "No ViewportContext exists for viewport %d", viewportId);

            slot.m_pipelineChanged = AZ::RPI::ViewportContext::PipelineChangedEvent::Handler(
                [this, i](AZ::RPI::RenderPipelinePtr)
                {
                    ApplyViewModes(m_slots[i]);
                });
            viewportContext && (viewportContext->ConnectCurrentPipelineChangedHandler(slot.m_pipelineChanged), true);

            const auto applyProjection = [viewportId](AzFramework::WindowSize size)
            {
                AZ::Matrix4x4 viewToClip;
                AZ::MakePerspectiveFovMatrixRH(
                    viewToClip, SandboxEditor::CameraDefaultFovRadians(),
                    aznumeric_cast<float>(AZStd::max(size.m_width, 1u)) / aznumeric_cast<float>(AZStd::max(size.m_height, 1u)),
                    SandboxEditor::CameraDefaultNearPlaneDistance(), SandboxEditor::CameraDefaultFarPlaneDistance(), true);
                auto* contextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
                AZ::RPI::ViewportContextPtr context = contextManager ? contextManager->GetViewportContextById(viewportId) : nullptr;
                context && (context->SetCameraProjectionMatrix(viewToClip), true);
            };
            slot.m_sizeChanged = AZ::RPI::ViewportContext::SizeChangedEvent::Handler(applyProjection);
            (i != 0 && viewportContext) &&
                (viewportContext->ConnectSizeChangedHandler(slot.m_sizeChanged),
                 applyProjection(viewportContext->GetViewportSize()), true);

            AZ::RPI::RenderPipelinePtr pipeline = viewportContext ? viewportContext->GetCurrentPipeline() : nullptr;
            pipeline && ((i == 0 ? pipeline->AddToRenderTick() : pipeline->RemoveFromRenderTick()), true);

            ApplyViewModes(slot);
        }

        m_dockAnchor = new AzQtComponents::StyledDockWidget(QStringLiteral("Viewport Dock Area"), this);
        m_dockAnchor->setObjectName(QStringLiteral("HammerViewportDockAnchor"));
        m_dockAnchor->setFeatures(QDockWidget::NoDockWidgetFeatures);
        m_dockAnchor->setTitleBarWidget(new QWidget(m_dockAnchor));
        m_dockAnchor->setWidget(new QWidget(m_dockAnchor));
        m_dockAnchor->hide();

        m_addViewportAction = new QAction(QIcon(QStringLiteral(":/Hammer/add-view.svg")), QObject::tr("Add Hammer Viewport"), this);
        m_addViewportAction->setObjectName(QStringLiteral("HammerAddViewportButton"));
        AZ_Warning(
            "Hammer", !m_addViewportAction->icon().isNull(),
            "add-view.svg did not load from the Hammer qrc; the add-viewport button will render blank");
        connect(
            m_addViewportAction, &QAction::triggered, this,
            [this]
            {
                Slot* unused = nullptr;
                for (Slot& slot : m_slots)
                {
                    const bool inUse = &slot == &m_slots[0] || slot.m_dock->isVisible() ||
                        AzQtComponents::DockTabWidget::IsTabbed(slot.m_dock);
                    unused = (!unused && !inUse) ? &slot : unused;
                }
                (unused && m_fancyDocking) &&
                    (m_fancyDocking->tabifyDockWidget(m_slots[0].m_dock, unused->m_dock, m_dockHost),
                     unused->m_engineViewport->show(), true);
            });

        m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, m_slots[0].m_dock);
        m_dockHost->resizeDocks({ m_slots[0].m_dock }, { m_dockHost->width() * 2 / 3 }, Qt::Horizontal);
        m_slots[0].m_dock->show();
        ActivateViewport(m_slots[0]);

        ViewportRequestBus::Handler::BusConnect();
        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusConnect();

        m_viewportUiOverlayWindow = m_slots[0].m_engineViewport->findChild<QWidget*>(QStringLiteral("ViewportUiWindow"));
        AZ_Warning(
            "Hammer", m_viewportUiOverlayWindow,
            "Could not find the ViewportUiWindow overlay; viewport UI will not follow the active viewport");
        m_viewportUiOverlayWindow && (m_viewportUiOverlayWindow->installEventFilter(this), true);

        m_overlaySyncTimer = new QTimer(this);
        connect(m_overlaySyncTimer, &QTimer::timeout, this, &ViewportLayout::SyncViewportUiOverlay);
        m_overlaySyncTimer->start(16);
    }

    ViewportLayout::~ViewportLayout()
    {
        AZ_Assert(
            ViewportRequestBus::Handler::BusIsConnected(),
            "ViewportLayout destroyed while its viewport-request bus was already disconnected");

        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusDisconnect();
        ViewportRequestBus::Handler::BusDisconnect();

        for (Slot& slot : m_slots)
        {
            slot.m_pipelineChanged.Disconnect();
            slot.m_sizeChanged.Disconnect();
            slot.m_toolBar && (delete slot.m_toolBar.data(), true);
        }

        m_slots[0].m_engineViewport && (m_slots[0].m_engineViewport->setParent(nullptr), true);

        for (Slot& slot : m_slots)
        {
            delete slot.m_dock;
            slot.m_dock = nullptr;
        }

        delete m_dockAnchor;
        m_dockAnchor = nullptr;
        delete m_addViewportButton;
    }

    void ViewportLayout::OnStartGameModeRequest()
    {
        AZ_Assert(!m_preGameModeActiveSlot, "Game mode started while a previous pre-game-mode viewport is still pending");
        m_preGameModeActiveSlot = m_activeSlot;
        auto* adoptedTabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(m_slots[0].m_dock);
        adoptedTabWidget && (adoptedTabWidget->setCurrentWidget(m_slots[0].m_dock), true);
        ActivateViewport(m_slots[0]);
        m_gameModeSuppressed = true;
        for (Slot& slot : m_slots)
        {
            ApplyViewModes(slot);
        }
    }

    void ViewportLayout::OnStopGameModeRequest()
    {
        m_gameModeSuppressed = false;
        for (Slot& slot : m_slots)
        {
            ApplyViewModes(slot);
        }
        auto* previousTabWidget = m_preGameModeActiveSlot
            ? AzQtComponents::DockTabWidget::ParentTabWidget(m_preGameModeActiveSlot->m_dock)
            : nullptr;
        previousTabWidget && (previousTabWidget->setCurrentWidget(m_preGameModeActiveSlot->m_dock), true);
        m_preGameModeActiveSlot && (ActivateViewport(*m_preGameModeActiveSlot), true);
        m_preGameModeActiveSlot = nullptr;
    }

    void ViewportLayout::ActivateViewport(Slot& slot)
    {
        AZ_Assert(slot.m_engineViewport, "ActivateViewport called on a slot with no engine viewport");
        m_activeSlot = &slot;
        slot.m_engineViewport->setFocus();
        static_cast<QtViewport*>(slot.m_engineViewport)->Update();
    }

    void ViewportLayout::ApplyViewModes(Slot& slot)
    {
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Assert(viewportContextManager, "ApplyViewModes called without the ViewportContextManager");
        AZ::RPI::ViewportContextPtr viewportContext = viewportContextManager
            ? viewportContextManager->GetViewportContextById(slot.m_engineViewport->GetViewportId())
            : nullptr;
        AZ::RPI::RenderPipelinePtr pipeline = viewportContext ? viewportContext->GetCurrentPipeline() : nullptr;

        AZ_Error(
            "Hammer", !pipeline || pipeline->FindFirstPass(AZ::Name("HammerWireframePass")),
            "Pipeline '%s' is missing the Hammer view-mode passes", pipeline ? pipeline->GetId().GetCStr() : "");

        const ViewModes viewModes = m_gameModeSuppressed ? ViewModes{} : slot.m_viewModes;
        const bool flatBackground = !viewModes.m_normal && (viewModes.m_wireframe || viewModes.m_overdraw);
        const struct
        {
            const char* m_pass;
            bool m_enabled;
        } passes[] = {
            { "OpaquePass", viewModes.m_normal },
            { "Forward", viewModes.m_normal },
            { "TransparentPass", viewModes.m_normal },
            { "HammerViewModeBackgroundPass", flatBackground },
            { "HammerWireframeHiddenPass", viewModes.m_wireframe },
            { "HammerWireframePass", viewModes.m_wireframe },
            { "HammerOverdrawCountPass", viewModes.m_overdraw },
            { "HammerOverdrawResolvePass", viewModes.m_overdraw },
        };
        for (const auto& entry : passes)
        {
            AZ::RPI::Ptr<AZ::RPI::Pass> pass = pipeline ? pipeline->FindFirstPass(AZ::Name(entry.m_pass)) : nullptr;
            pass && (pass->SetEnabled(entry.m_enabled), true);
        }
    }

    void ViewportLayout::SetCameraMirroringEnabled(bool enabled)
    {
        AZ_Assert(!enabled || m_overlaySyncTimer, "Camera mirroring enabled before the overlay sync timer exists; mirroring is driven by that timer");
        m_cameraMirroringEnabled = enabled;
    }

    void ViewportLayout::SyncViewportUiOverlay()
    {
        AZ_Assert(m_overlaySyncTimer, "SyncViewportUiOverlay ticked without its timer");
        AZ_Assert(m_dockAnchor, "SyncViewportUiOverlay ticked before the dock anchor was constructed");

        EditorViewportWidget* primaryViewport = EditorViewportWidget::GetPrimaryViewport();
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        for (Slot& slot : m_slots)
        {
            m_activeSlot = slot.m_engineViewport == primaryViewport ? &slot : m_activeSlot;
            AZ::RPI::ViewportContextPtr viewportContext = viewportContextManager
                ? viewportContextManager->GetViewportContextById(slot.m_engineViewport->GetViewportId())
                : nullptr;
            AZ::RPI::RenderPipelinePtr pipeline = viewportContext ? viewportContext->GetCurrentPipeline() : nullptr;
            pipeline &&
                ((slot.m_engineViewport->isVisible() ? pipeline->AddToRenderTick() : pipeline->RemoveFromRenderTick()), true);
            AZ::RPI::Ptr<AZ::RPI::Pass> iconPass = pipeline ? pipeline->FindFirstPass(AZ::Name("2DPass")) : nullptr;
            iconPass && (iconPass->SetEnabled(slot.m_engineViewport == primaryViewport), true);
        }

        QDockWidget* adoptedDock = m_slots[0].m_dock;

        bool anyDockDocked = false;
        const QList<QDockWidget*> hostDocks = m_dockHost->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QDockWidget* hostDock : hostDocks)
        {
            anyDockDocked = anyDockDocked ||
                (hostDock != m_dockAnchor && hostDock->isVisible() && !hostDock->isFloating() &&
                 hostDock->window() == m_dockHost->window());
        }

        (!adoptedDock->isVisible() && !AzQtComponents::DockTabWidget::IsTabbed(adoptedDock) &&
         adoptedDock->window() == m_dockHost->window()) &&
            (m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, adoptedDock), adoptedDock->show(), anyDockDocked = true, true);

        const bool adoptedDockedInHost =
            adoptedDock->isVisible() && !adoptedDock->isFloating() && adoptedDock->window() == m_dockHost->window();
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

        for (size_t i = 0; i < m_slots.size(); ++i)
        {
            QDockWidget* dock = m_slots[i].m_dock;
            auto* tabWidget = AzQtComponents::DockTabWidget::ParentTabWidget(dock);
            QTabBar* tabBar = tabWidget ? tabWidget->tabBar() : nullptr;
            QWidget* titleBar = !tabWidget ? dock->titleBarWidget() : nullptr;

            QPointer<QToolBar>& toolBar = m_slots[i].m_toolBar;
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

        const bool shouldChase = m_viewportUiOverlayWindow && m_activeSlot && m_activeSlot != &m_slots[0];

        shouldChase &&
            (m_viewportUiOverlayWindow->setGeometry(QRect(
                 m_activeSlot->m_engineViewport->mapToGlobal(QPoint(0, 0)), m_activeSlot->m_engineViewport->size())),
             true);

        QWidget* activeViewportWindow = m_activeSlot ? m_activeSlot->m_engineViewport->window() : nullptr;
        (m_viewportUiOverlayWindow && m_viewportUiOverlayWindow->isVisible() && activeViewportWindow &&
         activeViewportWindow->isActiveWindow()) &&
            (m_viewportUiOverlayWindow->raise(), true);

        AZ::RPI::ViewportContextPtr defaultContext;
        (m_cameraMirroringEnabled && viewportContextManager) &&
            (defaultContext = viewportContextManager->GetDefaultViewportContext(), true);

        const AZ::Transform mirrored = defaultContext ? defaultContext->GetCameraTransform() : AZ::Transform::CreateIdentity();
        for (Slot& slot : m_slots)
        {
            (defaultContext && slot.m_engineViewport != primaryViewport) &&
                (AtomToolsFramework::ModularViewportCameraControllerRequestBus::Event(
                     slot.m_engineViewport->GetViewportId(),
                     &AtomToolsFramework::ModularViewportCameraControllerRequests::StartTrackingTransform, mirrored),
                 true);
        }
    }

    bool ViewportLayout::eventFilter(QObject* watched, QEvent* event)
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

        const bool isOverlayMoveOrResize = watched == m_viewportUiOverlayWindow && m_activeSlot &&
            m_activeSlot != &m_slots[0] && (event->type() == QEvent::Move || event->type() == QEvent::Resize);

        isOverlayMoveOrResize &&
            (static_cast<QWidget*>(watched)->setGeometry(QRect(
                 m_activeSlot->m_engineViewport->mapToGlobal(QPoint(0, 0)), m_activeSlot->m_engineViewport->size())),
             true);

        return QWidget::eventFilter(watched, event);
    }
} // namespace Hammer
