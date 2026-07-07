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

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
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

        m_viewportLayoutWidget && (delete m_viewportLayoutWidget.data(), true);

        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();

        auto* featureProcessorFactory = AZ::RPI::FeatureProcessorFactory::Get();
        featureProcessorFactory && (featureProcessorFactory->UnregisterFeatureProcessor<HammerViewModeFeatureProcessor>(), true);

        HammerSystemComponent::Deactivate();
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

    void HammerEditorSystemComponent::EmbedViewportInCenter()
    {
        AZ_Assert(!m_viewportLayoutWidget, "EmbedViewportInCenter called while a viewport layout widget already exists");

        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Error("HammerEditorSystemComponent", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");

        AZ::RPI::ViewportContextPtr defaultContext;
        viewportContextManager && (defaultContext = viewportContextManager->GetDefaultViewportContext(), true);
        AZ_Error("HammerEditorSystemComponent", defaultContext, "Could not find the default ViewportContext to rename");
        defaultContext &&
            (viewportContextManager->RenameViewportContext(defaultContext, AZ::Name("Hammer Startup Placeholder")), true);

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

        viewPaneHost && (m_viewportLayoutWidget = new HammerViewportLayoutWidget(viewPaneHost), true);
        (m_viewportLayoutWidget && oldViewport) && (m_viewportLayoutWidget->AdoptRealPerspectiveViewport(*oldViewport), true);

        AZ_Warning(
            "HammerEditorSystemComponent", viewPaneHost && oldViewport,
            "Could not fully embed the Hammer viewports into the editor's dock space");
    }

    void HammerEditorSystemComponent::PrepareEditorChrome()
    {
        AZ_Assert(!m_viewportLayoutWidget, "PrepareEditorChrome should run once, before the viewports have been embedded");

        QSettings settings("O3DE", "O3DE");
        constexpr const char* MigratedStaleLayoutKey = "HammerGem/migratedStaleLayoutOnce";
        const bool alreadyMigrated = settings.value(MigratedStaleLayoutKey, false).toBool();
        (!alreadyMigrated) &&
            (settings.remove("ViewportLayout"), settings.remove("Editor/fancyWindowLayouts/last"),
             settings.setValue(MigratedStaleLayoutKey, true), true);
    }

} // namespace Hammer
