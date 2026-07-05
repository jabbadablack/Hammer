#include "HammerEditorSystemComponent.h"
#include "HammerViewportLayoutWidget.h"

#include "HammerQtEnvironment.h"

#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/array.h>

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <AzToolsFramework/ActionManager/Action/ActionManagerInterface.h>
#include <AzToolsFramework/ActionManager/HotKey/HotKeyManagerInterface.h>
#include <AzToolsFramework/Editor/ActionManagerIdentifiers/EditorContextIdentifiers.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

#include <QDockWidget>
#include <QIcon>
#include <QMainWindow>
#include <QSettings>
#include <QStatusBar>
#include <QToolButton>
#include <QWidget>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    namespace
    {
        constexpr const char* ViewportPaneName = "Hammer Viewport";

        struct ViewportCountButtonSpec
        {
            int m_count;
            const char* m_iconPath;
            const char* m_tooltip;
        };

        constexpr AZStd::array<ViewportCountButtonSpec, 3> ButtonSpecs = { {
            { 1, ":/Hammer/single-view.svg", "1 Viewport" },
            { 2, ":/Hammer/duo-view.svg", "2 Viewports" },
            { 4, ":/Hammer/quad-view.svg", "4 Viewports" },
        } };
    } // namespace

    AZ_COMPONENT_IMPL(HammerEditorSystemComponent, "HammerEditorSystemComponent",
        HammerEditorSystemComponentTypeId, BaseSystemComponent);

    void HammerEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
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

    HammerEditorSystemComponent::HammerEditorSystemComponent() = default;

    HammerEditorSystemComponent::~HammerEditorSystemComponent() = default;

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
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusConnect();
        HammerViewportRequestBus::Handler::BusConnect();

        PrepareEditorChrome();
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::SetIconsVisible(true);

        DestroyViewportCountButtons();

        m_viewportLayoutWidget && (RestoreViewportPaneToDockWidget(m_viewportLayoutWidget), true);

        AzToolsFramework::CloseViewPane(ViewportPaneName);
        AzToolsFramework::UnregisterViewPane(ViewportPaneName);
        m_viewportLayoutWidget = nullptr;

        HammerViewportRequestBus::Handler::BusDisconnect();
        AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::SetViewportCount(int count)
    {
        m_viewportLayoutWidget && (m_viewportLayoutWidget->SetViewportCount(count), true);
    }

    void HammerEditorSystemComponent::ToggleMaximizeActiveViewport()
    {
        m_viewportLayoutWidget && (m_viewportLayoutWidget->ToggleMaximizeActiveViewport(), true);
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        RegisterViewportPane();
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        EmbedViewportInCenter();
        CreateViewportCountButtons();
    }

    void HammerEditorSystemComponent::OnActionRegistrationHook()
    {
        struct HotkeyDef
        {
            const char* m_id;
            const char* m_name;
            const char* m_hotkey;
            int m_count;
        };
        constexpr HotkeyDef layoutHotkeys[] = {
            { "o3de.action.hammer.viewportLayoutSingle", "Single Viewport", "F1", 1 },
            { "o3de.action.hammer.viewportLayoutDual", "Dual Viewport", "F2", 2 },
            { "o3de.action.hammer.viewportLayoutQuad", "Quad Viewport", "F3", 4 },
        };

        for (const HotkeyDef& def : layoutHotkeys)
        {
            const AZStd::string description = AZStd::string::format("Switch Hammer to the %s layout", def.m_name);
            RegisterHotkeyAction(
                def.m_id, def.m_name, description.c_str(), "Viewport", def.m_hotkey,
                [this, count = def.m_count]
                {
                    SetViewportCount(count);
                });
        }

        RegisterHotkeyAction(
            "o3de.action.hammer.viewportToggleMaximize", "Toggle Maximize Viewport",
            "Maximize or restore the currently active Hammer viewport", "Viewport", "F4",
            [this]
            {
                ToggleMaximizeActiveViewport();
            });
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
                AZ_Assert(m_viewportLayoutWidget, "Failed to allocate HammerViewportLayoutWidget");
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

        QWidget* oldViewport = EmbedViewportPaneAsCentralWidget(
            ViewportPaneName,
            [this]() -> QWidget*
            {
                return m_viewportLayoutWidget;
            });

        (oldViewport && m_viewportLayoutWidget) && (m_viewportLayoutWidget->AdoptRealPerspectiveViewport(*oldViewport), true);
    }

    void HammerEditorSystemComponent::CreateViewportCountButtons()
    {
        AZ_Assert(m_viewportCountButtons.empty(), "CreateViewportCountButtons called while buttons already exist");

        QStatusBar* statusBar = GetMainWindowStatusBar();
        AZ_Error("HammerEditorSystemComponent", statusBar, "Could not find the Editor's status bar to host viewport-count buttons");

        (statusBar && m_viewportLayoutWidget) &&
            ([this, statusBar]
             {
                 for (const ViewportCountButtonSpec& spec : ButtonSpecs)
                 {
                     QToolButton* button = new QToolButton(statusBar);
                     button->setIcon(QIcon(QString::fromUtf8(spec.m_iconPath)));
                     button->setIconSize(QSize(16, 16));
                     button->setCheckable(true);
                     button->setAutoRaise(true);
                     button->setChecked(spec.m_count == 1);
                     button->setToolTip(QObject::tr(spec.m_tooltip));

                     QPointer<HammerViewportLayoutWidget> layoutWidget = m_viewportLayoutWidget;
                     const int count = spec.m_count;
                     QObject::connect(
                         button, &QToolButton::clicked, button,
                         [layoutWidget, count]
                         {
                             layoutWidget && (layoutWidget->SetViewportCount(count), true);
                         });

                     statusBar->addPermanentWidget(button);
                     m_viewportCountButtons.push_back(button);
                 }

                 QObject::connect(
                     m_viewportLayoutWidget, &HammerViewportLayoutWidget::ViewportCountChanged, m_viewportLayoutWidget,
                     [this](int count)
                     {
                         for (size_t i = 0; i < ButtonSpecs.size() && i < m_viewportCountButtons.size(); ++i)
                         {
                             QPointer<QToolButton>& button = m_viewportCountButtons[i];
                             button && (button->setChecked(ButtonSpecs[i].m_count == count), true);
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

    QWidget* HammerEditorSystemComponent::EmbedViewportPaneAsCentralWidget(
        const char* paneName, const AZStd::function<QWidget*()>& expectedContentAccessor)
    {
        AZ_Assert(paneName, "EmbedViewportPaneAsCentralWidget called with a null paneName");
        AZ_Assert(expectedContentAccessor, "EmbedViewportPaneAsCentralWidget called with an empty expectedContentAccessor");

        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        AZ_Error("HammerEditorSystemComponent", mainWindowWidget, "Could not get the Editor main window");

        QWidget* oldViewport = nullptr;
        mainWindowWidget && (oldViewport = FindMainEditorViewport(mainWindowWidget), true);
        AZ_Error("HammerEditorSystemComponent", oldViewport, "Could not find the main EditorViewportWidget");

        QWidget* startAncestor = nullptr;
        oldViewport && (startAncestor = oldViewport->parentWidget(), true);

        QMainWindow* viewPaneHost = nullptr;
        for (QWidget* ancestor = startAncestor; ancestor && !viewPaneHost; ancestor = ancestor->parentWidget())
        {
            viewPaneHost = qobject_cast<QMainWindow*>(ancestor);
        }
        AZ_Error("HammerEditorSystemComponent", viewPaneHost, "Could not find the QMainWindow hosting the main viewport");

        QWidget* oldCentralWidget = nullptr;
        viewPaneHost && (oldCentralWidget = viewPaneHost->centralWidget(), true);

        viewPaneHost && (AzToolsFramework::OpenViewPane(paneName), true);

        QWidget* content = nullptr;
        viewPaneHost && (content = AzToolsFramework::GetViewPaneWidget<QWidget>(paneName), true);
        AZ_Error("HammerEditorSystemComponent", content, "Could not open the '%s' view pane", paneName);

        QWidget* expectedContent = nullptr;
        content && (expectedContent = expectedContentAccessor(), true);
        AZ_Assert(
            !content || content == expectedContent, "Opened '%s' view pane's content is not the expected widget", paneName);

        QDockWidget* paneDockWidget = nullptr;
        content && (paneDockWidget = qobject_cast<QDockWidget*>(content->parentWidget()), true);
        AZ_Error("HammerEditorSystemComponent", paneDockWidget, "Could not find the dock widget hosting the '%s' view pane", paneName);
        m_paneDockWidget = paneDockWidget;

        (viewPaneHost && paneDockWidget) &&
            (viewPaneHost->removeDockWidget(paneDockWidget), paneDockWidget->hide(), paneDockWidget->setWidget(nullptr), true);

        (viewPaneHost && content) && (viewPaneHost->setCentralWidget(content), content->show(), true);

        (oldCentralWidget && oldCentralWidget != content) &&
            (oldCentralWidget->hide(), oldCentralWidget->setParent(nullptr), true);

        const bool succeeded = viewPaneHost && oldViewport && content && paneDockWidget;
        AZ_Warning(
            "HammerEditorSystemComponent", succeeded, "Could not fully embed the '%s' view pane as the central widget", paneName);
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
        const char* actionId, const char* name, const char* description, const char* category, const char* hotkey,
        AZStd::function<void()> callback)
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
        actionProperties.m_category = category;

        (actionManagerInterface && hotKeyManagerInterface) &&
            (actionManagerInterface->RegisterAction(
                 EditorIdentifiers::MainWindowActionContextIdentifier, actionId, actionProperties, AZStd::move(callback)),
             hotKeyManagerInterface->SetActionHotKey(actionId, hotkey), true);
    }

    QStatusBar* HammerEditorSystemComponent::GetMainWindowStatusBar() const
    {
        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        auto* mainWindow = qobject_cast<QMainWindow*>(mainWindowWidget);
        AZ_Error(
            "HammerEditorSystemComponent", mainWindow, "Could not find the Editor's main QMainWindow to host viewport-count buttons");

        QStatusBar* statusBar = nullptr;
        mainWindow && (statusBar = mainWindow->statusBar(), true);
        AZ_Error("HammerEditorSystemComponent", statusBar, "Editor main window has no status bar");
        return statusBar;
    }

} // namespace Hammer
