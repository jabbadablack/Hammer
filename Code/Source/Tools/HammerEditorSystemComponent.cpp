#include "HammerEditorSystemComponent.h"
#include "HammerViewModeFeatureProcessor.h"
#include "HammerViewportLayoutWidget.h"

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>

#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInterface.h>
#include <AzToolsFramework/ActionManager/HotKey/HotKeyManagerInterface.h>
#include <AzToolsFramework/ActionManager/ToolBar/ToolBarManagerInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorContextIdentifiers.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorToolBarIdentifiers.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

#include <QDockWidget>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>
#include <QStatusBar>
#include <QToolButton>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    namespace
    {
        constexpr const char* ViewportPaneName = "Hammer Viewport";

        struct ViewportLayoutSpec
        {
            int m_count;
            const char* m_iconPath;
            const char* m_tooltip;
            const char* m_actionId;
            const char* m_actionName;
            const char* m_hotkey;
        };

        constexpr AZStd::array<ViewportLayoutSpec, 3> LayoutSpecs = { {
            { 1, ":/Hammer/single-view.svg", "1 Viewport", "o3de.action.hammer.viewportLayoutSingle", "Single Viewport", "F1" },
            { 2, ":/Hammer/duo-view.svg", "2 Viewports", "o3de.action.hammer.viewportLayoutDual", "Dual Viewport", "F2" },
            { 4, ":/Hammer/quad-view.svg", "4 Viewports", "o3de.action.hammer.viewportLayoutQuad", "Quad Viewport", "F3" },
        } };

        QWidget* FindMainEditorViewport(QWidget& root)
        {
            const QList<QWidget*> children = root.findChildren<QWidget*>();
            const auto it = AZStd::find_if(
                children.begin(), children.end(),
                [](QWidget* child)
                {
                    return qstrcmp(child->metaObject()->className(), "EditorViewportWidget") == 0;
                });
            return it != children.end() ? *it : nullptr;
        }
    } // namespace

    AZ_COMPONENT_IMPL(HammerEditorSystemComponent, "HammerEditorSystemComponent",
        HammerEditorSystemComponentTypeId, BaseSystemComponent);

    void HammerEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        HammerViewModeFeatureProcessor::Reflect(context);

        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        serializeContext &&
            (serializeContext->Class<HammerEditorSystemComponent, HammerSystemComponent>()->Version(0), true);

        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        behaviorContext &&
            (behaviorContext->EBus<HammerViewportRequestBus>("HammerViewportRequestBus")
                 ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                 ->Attribute(AZ::Script::Attributes::Category, "Editor")
                 ->Attribute(AZ::Script::Attributes::Module, "hammer")
                 ->Event("SetViewportCount", &HammerViewportRequestBus::Events::SetViewportCount)
                 ->Event("ToggleMaximizeActiveViewport", &HammerViewportRequestBus::Events::ToggleMaximizeActiveViewport),
             true);
    }

    void HammerEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("HammerEditorService"));
    }

    void HammerEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("HammerEditorService"));
    }

    void HammerEditorSystemComponent::Activate()
    {
        HammerSystemComponent::Activate();

        auto* featureProcessorFactory = AZ::RPI::FeatureProcessorFactory::Get();
        AZ_Assert(featureProcessorFactory, "HammerEditorSystemComponent activated before the FeatureProcessorFactory");
        featureProcessorFactory->RegisterFeatureProcessor<HammerViewModeFeatureProcessor>();

        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusConnect();

        PrepareEditorChrome();
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::SetIconsVisible(true);

        DestroyViewportCountButtons();
        m_viewModeButtons.clear();

        m_viewportLayoutWidget && (RestoreViewportPaneToDockWidget(m_viewportLayoutWidget), true);

        AzToolsFramework::CloseViewPane(ViewportPaneName);
        AzToolsFramework::UnregisterViewPane(ViewportPaneName);
        m_viewportLayoutWidget = nullptr;

        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();

        auto* featureProcessorFactory = AZ::RPI::FeatureProcessorFactory::Get();
        featureProcessorFactory && (featureProcessorFactory->UnregisterFeatureProcessor<HammerViewModeFeatureProcessor>(), true);

        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        RegisterViewportPane();
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        EmbedViewportInCenter();
        CreateViewportCountButtons();
        ConnectViewModeSwitcherSync();
    }

    void HammerEditorSystemComponent::OnActionRegistrationHook()
    {
        for (const ViewportLayoutSpec& spec : LayoutSpecs)
        {
            const AZStd::string description = AZStd::string::format("Switch Hammer to the %s layout", spec.m_actionName);
            RegisterHotkeyAction(
                spec.m_actionId, spec.m_actionName, description.c_str(), spec.m_hotkey,
                [count = spec.m_count]
                {
                    HammerViewportRequestBus::Broadcast(&HammerViewportRequests::SetViewportCount, count);
                });
        }

        RegisterHotkeyAction(
            "o3de.action.hammer.viewportToggleMaximize", "Toggle Maximize Viewport",
            "Maximize or restore the currently active Hammer viewport", "F4",
            []
            {
                HammerViewportRequestBus::Broadcast(&HammerViewportRequests::ToggleMaximizeActiveViewport);
            });
    }

    void HammerEditorSystemComponent::OnWidgetActionRegistrationHook()
    {
        auto* actionManagerInterface = AZ::Interface<AzToolsFramework::ActionManagerInterface>::Get();
        AZ_Error("HammerEditorSystemComponent", actionManagerInterface, "Could not find AzToolsFramework::ActionManagerInterface");

        AzToolsFramework::WidgetActionProperties widgetActionProperties;
        widgetActionProperties.m_name = "Hammer View Mode";
        widgetActionProperties.m_category = "Viewport";

        actionManagerInterface &&
            (actionManagerInterface->RegisterWidgetAction(
                 "o3de.widgetAction.hammer.viewMode", widgetActionProperties,
                 [this]
                 {
                     return CreateViewModeToolBarButton();
                 }),
             true);
    }

    void HammerEditorSystemComponent::OnToolBarBindingHook()
    {
        auto* toolBarManagerInterface = AZ::Interface<AzToolsFramework::ToolBarManagerInterface>::Get();
        AZ_Error("HammerEditorSystemComponent", toolBarManagerInterface, "Could not find AzToolsFramework::ToolBarManagerInterface");

        toolBarManagerInterface &&
            (toolBarManagerInterface->AddWidgetToToolBar(
                 EditorIdentifiers::ViewportTopToolBarIdentifier, "o3de.widgetAction.hammer.viewMode", 450),
             true);
    }

    QWidget* HammerEditorSystemComponent::CreateViewModeToolBarButton()
    {
        QToolButton* button = new QToolButton();
        button->setIcon(QIcon(QStringLiteral(":/Hammer/toolbar_icon.svg")));
        button->setToolTip(QObject::tr("View Mode"));
        button->setPopupMode(QToolButton::InstantPopup);
        button->setAutoRaise(true);

        QMenu* menu = new QMenu(button);
        QAction* normalAction = menu->addAction(QObject::tr("Normal"));
        QAction* wireframeAction = menu->addAction(QObject::tr("Wireframe"));
        QAction* overdrawAction = menu->addAction(QObject::tr("Quad Overdraw"));
        for (QAction* action : { normalAction, wireframeAction, overdrawAction })
        {
            action->setCheckable(true);
            QObject::connect(
                action, &QAction::toggled, button,
                [normalAction, wireframeAction, overdrawAction]
                {
                    HammerViewportRequestBus::Broadcast(
                        &HammerViewportRequests::SetActiveViewportViewModes, normalAction->isChecked(),
                        wireframeAction->isChecked(), overdrawAction->isChecked());
                });
        }
        normalAction->setChecked(true);
        button->setMenu(menu);

        m_viewModeButtons.push_back(button);
        ConnectViewModeSwitcherSync();
        return button;
    }

    void HammerEditorSystemComponent::ConnectViewModeSwitcherSync()
    {
        for (QPointer<QToolButton>& button : m_viewModeButtons)
        {
            const bool canConnect =
                m_viewportLayoutWidget && button && !button->property("hammerViewModeSynced").toBool();
            canConnect &&
                (QObject::connect(
                     m_viewportLayoutWidget, &HammerViewportLayoutWidget::ActiveViewModesChanged, button,
                     [button = button.data()](bool normal, bool wireframe, bool overdraw)
                     {
                         const QList<QAction*> actions = button->menu()->actions();
                         (actions.size() == 3) &&
                             (actions[0]->setChecked(normal), actions[1]->setChecked(wireframe),
                              actions[2]->setChecked(overdraw), true);
                     }),
                 button->setProperty("hammerViewModeSynced", true), true);
        }
    }

    void HammerEditorSystemComponent::RegisterViewportPane()
    {
        AzToolsFramework::ViewPaneOptions viewOptions;
        viewOptions.isDeletable = true;
        viewOptions.showInMenu = false;
        viewOptions.isDisabledInComponentMode = false;

        AzToolsFramework::RegisterViewPane<QWidget>(
            ViewportPaneName, "Viewport", viewOptions,
            [this](QWidget* parent) -> QWidget*
            {
                AZ_Assert(!m_viewportLayoutWidget, "The '%s' view pane's widget was requested a second time", ViewportPaneName);
                m_viewportLayoutWidget = new HammerViewportLayoutWidget(parent);
                return m_viewportLayoutWidget;
            });
    }

    void HammerEditorSystemComponent::EmbedViewportInCenter()
    {
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerEditorSystemComponent", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");

        AZ::RPI::ViewportContextPtr defaultContext;
        viewportContextManager && (defaultContext = viewportContextManager->GetDefaultViewportContext(), true);
        AZ_Error("HammerEditorSystemComponent", defaultContext, "Could not find the default ViewportContext to rename");
        defaultContext &&
            (viewportContextManager->RenameViewportContext(defaultContext, AZ::Name("Hammer Startup Placeholder")), true);

        QWidget* oldViewport = EmbedViewportPaneAsCentralWidget();
        (oldViewport && m_viewportLayoutWidget) && (m_viewportLayoutWidget->AdoptRealPerspectiveViewport(*oldViewport), true);
    }

    void HammerEditorSystemComponent::CreateViewportCountButtons()
    {
        AZ_Assert(m_viewportCountButtons.empty(), "CreateViewportCountButtons called while buttons already exist");

        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        auto* mainWindow = qobject_cast<QMainWindow*>(mainWindowWidget);
        QStatusBar* statusBar = mainWindow ? mainWindow->statusBar() : nullptr;
        AZ_Error("HammerEditorSystemComponent", statusBar, "Could not find the Editor's status bar to host viewport-count buttons");

        (statusBar && m_viewportLayoutWidget) &&
            ([this, statusBar]
             {
                 for (const ViewportLayoutSpec& spec : LayoutSpecs)
                 {
                     QToolButton* button = new QToolButton(statusBar);
                     button->setIcon(QIcon(QString::fromUtf8(spec.m_iconPath)));
                     button->setIconSize(QSize(16, 16));
                     button->setCheckable(true);
                     button->setAutoRaise(true);
                     button->setChecked(spec.m_count == 1);
                     button->setToolTip(QObject::tr(spec.m_tooltip));

                     QObject::connect(
                         button, &QToolButton::clicked, button,
                         [count = spec.m_count]
                         {
                             HammerViewportRequestBus::Broadcast(&HammerViewportRequests::SetViewportCount, count);
                         });

                     statusBar->addPermanentWidget(button);
                     m_viewportCountButtons.push_back(button);
                 }

                 QObject::connect(
                     m_viewportLayoutWidget, &HammerViewportLayoutWidget::ViewportCountChanged, m_viewportLayoutWidget,
                     [this](int count)
                     {
                         for (size_t i = 0; i < LayoutSpecs.size() && i < m_viewportCountButtons.size(); ++i)
                         {
                             QPointer<QToolButton>& button = m_viewportCountButtons[i];
                             button && (button->setChecked(LayoutSpecs[i].m_count == count), true);
                         }
                     });
             }(),
             true);
    }

    void HammerEditorSystemComponent::DestroyViewportCountButtons()
    {
        m_viewportLayoutWidget &&
            (QObject::disconnect(
                 m_viewportLayoutWidget, &HammerViewportLayoutWidget::ViewportCountChanged, m_viewportLayoutWidget, nullptr),
             true);

        for (QPointer<QToolButton>& button : m_viewportCountButtons)
        {
            button && (delete button, true);
        }
        m_viewportCountButtons.clear();
    }

    void HammerEditorSystemComponent::PrepareEditorChrome()
    {
        AZ_Assert(!m_paneDockWidget, "PrepareEditorChrome should run once, before any view pane has been embedded");

        QSettings settings("O3DE", "O3DE");
        constexpr const char* MigratedStaleLayoutKey = "HammerGem/migratedStaleLayoutOnce";
        const bool alreadyMigrated = settings.value(MigratedStaleLayoutKey, false).toBool();
        (!alreadyMigrated) &&
            (settings.remove("ViewportLayout"), settings.remove("Editor/fancyWindowLayouts/last"),
             settings.setValue(MigratedStaleLayoutKey, true), true);
    }

    QWidget* HammerEditorSystemComponent::EmbedViewportPaneAsCentralWidget()
    {
        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        AZ_Error("HammerEditorSystemComponent", mainWindowWidget, "Could not get the Editor main window");

        QWidget* oldViewport = nullptr;
        mainWindowWidget && (oldViewport = FindMainEditorViewport(*mainWindowWidget), true);
        AZ_Error("HammerEditorSystemComponent", oldViewport, "Could not find the main EditorViewportWidget");

        QMainWindow* viewPaneHost = nullptr;
        QWidget* startAncestor = oldViewport ? oldViewport->parentWidget() : nullptr;
        for (QWidget* ancestor = startAncestor; ancestor && !viewPaneHost; ancestor = ancestor->parentWidget())
        {
            viewPaneHost = qobject_cast<QMainWindow*>(ancestor);
        }
        AZ_Error("HammerEditorSystemComponent", viewPaneHost, "Could not find the QMainWindow hosting the main viewport");

        QWidget* oldCentralWidget = nullptr;
        viewPaneHost && (oldCentralWidget = viewPaneHost->centralWidget(), true);

        viewPaneHost && (AzToolsFramework::OpenViewPane(ViewportPaneName), true);

        QWidget* content = nullptr;
        viewPaneHost && (content = AzToolsFramework::GetViewPaneWidget<QWidget>(ViewportPaneName), true);
        AZ_Error("HammerEditorSystemComponent", content, "Could not open the '%s' view pane", ViewportPaneName);
        AZ_Assert(
            !content || content == m_viewportLayoutWidget, "Opened '%s' view pane's content is not the expected widget",
            ViewportPaneName);

        QDockWidget* paneDockWidget = nullptr;
        content && (paneDockWidget = qobject_cast<QDockWidget*>(content->parentWidget()), true);
        AZ_Error("HammerEditorSystemComponent", paneDockWidget, "Could not find the dock widget hosting the '%s' view pane", ViewportPaneName);
        m_paneDockWidget = paneDockWidget;

        (viewPaneHost && paneDockWidget) &&
            (viewPaneHost->removeDockWidget(paneDockWidget), paneDockWidget->hide(), paneDockWidget->setWidget(nullptr), true);

        (viewPaneHost && content) && (viewPaneHost->setCentralWidget(content), content->show(), true);

        (oldCentralWidget && oldCentralWidget != content) &&
            (oldCentralWidget->hide(), oldCentralWidget->setParent(nullptr), true);

        const bool succeeded = viewPaneHost && oldViewport && content && paneDockWidget;
        AZ_Warning(
            "HammerEditorSystemComponent", succeeded, "Could not fully embed the '%s' view pane as the central widget", ViewportPaneName);
        return succeeded ? oldViewport : nullptr;
    }

    void HammerEditorSystemComponent::RestoreViewportPaneToDockWidget(QWidget* content)
    {
        AZ_Assert(content, "RestoreViewportPaneToDockWidget called with a null content widget");
        AZ_Warning(
            "HammerEditorSystemComponent", m_paneDockWidget,
            "No dock widget was captured to restore the view pane's content into; it may already have been closed");

        (m_paneDockWidget && content) && (content->hide(), m_paneDockWidget->setWidget(content), true);
    }

    void HammerEditorSystemComponent::RegisterHotkeyAction(
        const char* actionId, const char* name, const char* description, const char* hotkey, AZStd::function<void()> callback)
    {
        AZ_Assert(actionId, "RegisterHotkeyAction called with a null actionId");
        AZ_Assert(callback, "RegisterHotkeyAction called with an empty callback");

        auto* actionManagerInterface = AZ::Interface<AzToolsFramework::ActionManagerInterface>::Get();
        auto* hotKeyManagerInterface = AZ::Interface<AzToolsFramework::HotKeyManagerInterface>::Get();
        AZ_Error("HammerEditorSystemComponent", actionManagerInterface, "Could not find AzToolsFramework::ActionManagerInterface");
        AZ_Error("HammerEditorSystemComponent", hotKeyManagerInterface, "Could not find AzToolsFramework::HotKeyManagerInterface");

        AzToolsFramework::ActionProperties actionProperties;
        actionProperties.m_name = name;
        actionProperties.m_description = description;
        actionProperties.m_category = "Viewport";

        (actionManagerInterface && hotKeyManagerInterface) &&
            (actionManagerInterface->RegisterAction(
                 EditorIdentifiers::MainWindowActionContextIdentifier, actionId, actionProperties, AZStd::move(callback)),
             hotKeyManagerInterface->SetActionHotKey(actionId, hotkey), true);
    }
} // namespace Hammer
