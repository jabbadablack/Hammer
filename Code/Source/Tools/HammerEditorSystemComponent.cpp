#include "HammerEditorSystemComponent.h"
#include "HammerViewModeFeatureProcessor.h"
#include "HammerViewportLayoutWidget.h"

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInterface.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInternalInterface.h>
#include <AzToolsFramework/ActionManager/ToolBar/ToolBarManagerInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorToolBarIdentifiers.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

#include <QAction>
#include <QApplication>
#include <QDataStream>
#include <QDockWidget>
#include <QFile>
#include <QIcon>
#include <QIODevice>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>
#include <QTimer>
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

    void HammerEditorSystemComponent::NotifyCentralWidgetInitialized()
    {
        auto* fileIo = AZ::IO::FileIOBase::GetInstance();
        AZ_Assert(fileIo, "NotifyCentralWidgetInitialized fired before FileIOBase is available");
        char bundledLayoutPath[AZ_MAX_PATH_LEN] = { 0 };
        const bool bundledPathResolved =
            fileIo && fileIo->ResolvePath("@gemroot:Hammer@/Editor/hammer_layout.ini", bundledLayoutPath, AZ_MAX_PATH_LEN);
        AZ_Warning(
            "HammerEditorSystemComponent", bundledPathResolved,
            "Could not resolve @gemroot:Hammer@; the bundled Hammer layout will not be imported");

        QSettings settings("O3DE", "O3DE");
        QVariant bundledLayout;
        (bundledPathResolved && !settings.contains("Editor/fancyWindowLayouts/hammer_layout")) &&
            (bundledLayout = QSettings(QString::fromUtf8(bundledLayoutPath), QSettings::IniFormat).value("hammer_layout"), true);
        bundledLayout.isValid() &&
            (settings.setValue("Editor/fancyWindowLayouts/hammer_layout", bundledLayout), settings.sync(), true);

        m_seedHammerLayout = !settings.contains("Editor/fancyWindowLayouts/last") &&
            settings.contains("Editor/fancyWindowLayouts/hammer_layout");

        EmbedViewportInCenter();
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        ConnectViewModeSwitcherSync();

        auto* actionManagerInterface = AZ::Interface<AzToolsFramework::ActionManagerInterface>::Get();
        auto* actionManagerInternalInterface = AZ::Interface<AzToolsFramework::ActionManagerInternalInterface>::Get();
        AZ_Error(
            "HammerEditorSystemComponent", actionManagerInterface && actionManagerInternalInterface,
            "Could not find the ActionManager interfaces; the Hammer startup layout will not be applied");

        const bool hammerLayoutSaved = QSettings("O3DE", "O3DE").contains("Editor/fancyWindowLayouts/hammer_layout");
        AZ_Warning(
            "HammerEditorSystemComponent", hammerLayoutSaved,
            "No saved 'hammer_layout' found; the editor will keep its stock default layout");

        QAction* restoreDefaultAction =
            actionManagerInternalInterface ? actionManagerInternalInterface->GetAction("o3de.action.layout.restoreDefault") : nullptr;
        AZ_Warning(
            "HammerEditorSystemComponent", restoreDefaultAction || !hammerLayoutSaved,
            "Could not find the Restore Default Layout action to redirect to 'hammer_layout'");

        (restoreDefaultAction && hammerLayoutSaved && actionManagerInterface) &&
            (QObject::disconnect(restoreDefaultAction, &QAction::triggered, nullptr, nullptr),
             QObject::connect(
                 restoreDefaultAction, &QAction::triggered, restoreDefaultAction,
                 [actionManagerInterface]
                 {
                     actionManagerInterface->TriggerAction("o3de.action.layout[hammer_layout].load");
                 }),
             true);

        (m_seedHammerLayout && hammerLayoutSaved && actionManagerInterface) &&
            (QTimer::singleShot(
                 0,
                 [actionManagerInterface]
                 {
                     actionManagerInterface->TriggerAction("o3de.action.layout[hammer_layout].load");
                 }),
             true);

        const auto exportHammerLayout = []
        {
            auto* fileIo = AZ::IO::FileIOBase::GetInstance();
            char bundledLayoutPath[AZ_MAX_PATH_LEN] = { 0 };
            const bool bundledPathResolved =
                fileIo && fileIo->ResolvePath("@gemroot:Hammer@/Editor/hammer_layout.ini", bundledLayoutPath, AZ_MAX_PATH_LEN);
            AZ_Warning(
                "HammerEditorSystemComponent", bundledPathResolved,
                "Could not resolve @gemroot:Hammer@; the Hammer layout will not be exported with the gem");

            const QVariant userLayout = QSettings("O3DE", "O3DE").value("Editor/fancyWindowLayouts/hammer_layout");
            const auto serializeVariant = [](const QVariant& value)
            {
                QByteArray bytes;
                QDataStream stream(&bytes, QIODevice::WriteOnly);
                stream << value;
                return bytes;
            };
            const bool layoutChanged = bundledPathResolved && userLayout.isValid() &&
                serializeVariant(QSettings(QString::fromUtf8(bundledLayoutPath), QSettings::IniFormat).value("hammer_layout")) !=
                    serializeVariant(userLayout);
            layoutChanged &&
                (fileIo->CreatePath("@gemroot:Hammer@/Editor"),
                 QSettings(QString::fromUtf8(bundledLayoutPath), QSettings::IniFormat).setValue("hammer_layout", userLayout),
                 true);
            AZ_Warning(
                "HammerEditorSystemComponent", !layoutChanged || QFile::exists(QString::fromUtf8(bundledLayoutPath)),
                "Failed to write the bundled Hammer layout file");
        };
        exportHammerLayout();

        QAction* saveHammerLayoutAction = actionManagerInternalInterface
            ? actionManagerInternalInterface->GetAction("o3de.action.layout[hammer_layout].save")
            : nullptr;
        saveHammerLayoutAction &&
            (QObject::connect(saveHammerLayoutAction, &QAction::triggered, saveHammerLayoutAction, exportHammerLayout), true);
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
        const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
        const auto mainWindowIt = AZStd::find_if(
            topLevelWidgets.begin(), topLevelWidgets.end(),
            [](QWidget* widget)
            {
                return qstrcmp(widget->metaObject()->className(), "MainWindow") == 0;
            });
        mainWindowWidget = (!mainWindowWidget && mainWindowIt != topLevelWidgets.end()) ? *mainWindowIt : mainWindowWidget;
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

        settings.remove("Editor/fancyWindowLayouts/hammer_last");
    }

} // namespace Hammer
