#include "HammerEditorSystemComponent.h"
#include "HammerViewModeFeatureProcessor.h"
#include "HammerViewportLayoutWidget.h"

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>

#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInterface.h>
#include <AzToolsFramework/ActionManager/ToolBar/ToolBarManagerInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorToolBarIdentifiers.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

#include <QDockWidget>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>
#include <QToolButton>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    namespace
    {
        constexpr const char* ViewportPaneName = "Hammer Viewport";

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
                 ->Event("SetCameraMirroringEnabled", &HammerViewportRequestBus::Events::SetCameraMirroringEnabled),
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
        ConnectViewModeSwitcherSync();
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

        menu->addSeparator();
        QAction* mirrorAction = menu->addAction(QObject::tr("Mirror Main Camera"));
        mirrorAction->setCheckable(true);
        QObject::connect(
            mirrorAction, &QAction::toggled, button,
            [](bool checked)
            {
                HammerViewportRequestBus::Broadcast(&HammerViewportRequests::SetCameraMirroringEnabled, checked);
            });

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
                         (actions.size() >= 3) &&
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
} // namespace Hammer
