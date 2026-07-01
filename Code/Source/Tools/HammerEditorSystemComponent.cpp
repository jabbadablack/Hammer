
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/string/string_view.h>

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QWidget>

#include "HammerEditorSystemComponent.h"
#include "HammerViewportLayoutWidget.h"
#include "HammerWireframeFeatureProcessor.h"

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    namespace
    {
        constexpr const char* ViewportPaneName = "Hammer Viewport";
    }

    AZ_COMPONENT_IMPL(HammerEditorSystemComponent, "HammerEditorSystemComponent",
        HammerEditorSystemComponentTypeId, BaseSystemComponent);

    void HammerEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        HammerWireframeFeatureProcessor::Reflect(context);

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<HammerEditorSystemComponent, HammerSystemComponent>()
                ->Version(0);
        }
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

    void HammerEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void HammerEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void HammerEditorSystemComponent::Activate()
    {
        HammerSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();

        AZ::RPI::FeatureProcessorFactory::Get()->RegisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        // Clears a stale "Layout" value an earlier version of this Gem left in the Windows
        // registry, which otherwise leaves CLayoutWnd in a broken state at startup.
        QSettings settings;
        settings.remove("ViewportLayout");
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        // EmbedViewportInCenter() pulled m_viewportLayoutWidget out of m_paneDockWidget to use as
        // the Editor's central widget directly; give it back before closing the pane through
        // QtViewPaneManager, since QtViewPane::CloseInstance() sends a QCloseEvent to
        // dockWidget->widget() - if that were still null (or pointing at nothing), the close path
        // would dispatch to a null receiver.
        if (m_paneDockWidget && m_viewportLayoutWidget)
        {
            m_viewportLayoutWidget->hide();
            m_paneDockWidget->setWidget(m_viewportLayoutWidget);
        }

        // Closing/unregistering the pane lets QtViewPaneManager drive teardown order, the same
        // way it does for every other Editor pane, instead of us manually nulling widget pointers.
        AzToolsFramework::CloseViewPane(ViewportPaneName);
        AzToolsFramework::UnregisterViewPane(ViewportPaneName);
        m_paneDockWidget = nullptr;
        m_viewportLayoutWidget = nullptr;
        HammerWireframeFeatureProcessor::ClearWireframeWindows();

        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        // This is the Editor-startup point where the EditorRequests bus handler that actually
        // implements pane registration (SandboxIntegrationManager) is guaranteed to be connected;
        // registering from Activate() is too early and silently no-ops.
        RegisterViewportPane();
    }

    void HammerEditorSystemComponent::NotifyEditorInitialized()
    {
        EmbedViewportInCenter();
    }

    void HammerEditorSystemComponent::RegisterViewportPane()
    {
        AzToolsFramework::ViewPaneOptions viewOptions;
        viewOptions.isDeletable = true;
        viewOptions.showInMenu = true;
        viewOptions.isDisabledInComponentMode = false;

        AzToolsFramework::RegisterViewPane<QWidget>(
            ViewportPaneName,
            "Viewport",
            viewOptions,
            [this](QWidget* parent) -> QWidget*
            {
                m_viewportLayoutWidget = new HammerViewportLayoutWidget(parent);
                return m_viewportLayoutWidget;
            });
    }

    void HammerEditorSystemComponent::EmbedViewportInCenter()
    {
        AZ_Warning("HammerEditorSystemComponent", false, "EmbedViewportInCenter: starting");

        QWidget* mainWindowWidget = nullptr;
        AzToolsFramework::EditorRequestBus::BroadcastResult(mainWindowWidget, &AzToolsFramework::EditorRequests::GetMainWindow);
        AZ_Warning("HammerEditorSystemComponent", mainWindowWidget, "Could not get the Editor main window");
        if (!mainWindowWidget)
        {
            return;
        }

        // EditorViewportWidget is a private Editor class, so it's found generically by its Qt
        // class name rather than by including its header. It's not a registered view pane (the
        // real Editor never registers it as one - it's created directly as part of CLayoutWnd),
        // so this walk is the only way to locate the QMainWindow that hosts it.
        QWidget* oldViewport = nullptr;
        for (QWidget* child : mainWindowWidget->findChildren<QWidget*>())
        {
            if (AZStd::string_view(child->metaObject()->className()) == "EditorViewportWidget")
            {
                oldViewport = child;
                break;
            }
        }
        AZ_Warning("HammerEditorSystemComponent", oldViewport, "Could not find the main EditorViewportWidget");
        if (!oldViewport)
        {
            return;
        }
        AZ_Warning("HammerEditorSystemComponent", false, "EmbedViewportInCenter: found EditorViewportWidget");

        // Walk up to the QMainWindow whose central widget hosts the viewport (Code/Editor/MainWindow.cpp: m_viewPaneHost).
        // This is the same QMainWindow that QtViewPaneManager docks every other pane into.
        QMainWindow* viewPaneHost = nullptr;
        for (QWidget* ancestor = oldViewport->parentWidget(); ancestor; ancestor = ancestor->parentWidget())
        {
            if (QMainWindow* mainWindow = qobject_cast<QMainWindow*>(ancestor))
            {
                viewPaneHost = mainWindow;
                break;
            }
        }
        AZ_Warning("HammerEditorSystemComponent", viewPaneHost, "Could not find the QMainWindow hosting the main viewport");
        if (!viewPaneHost)
        {
            return;
        }
        AZ_Warning("HammerEditorSystemComponent", false, "EmbedViewportInCenter: found viewPaneHost");

        m_paneDockWidget = AzToolsFramework::InstanceViewPane(ViewportPaneName);
        AZ_Warning("HammerEditorSystemComponent", m_paneDockWidget, "Could not instance the '%s' view pane", ViewportPaneName);
        if (!m_paneDockWidget)
        {
            return;
        }

        QWidget* content = m_paneDockWidget->widget();
        AZ_Warning("HammerEditorSystemComponent", content, "Instanced '%s' view pane has no content widget", ViewportPaneName);
        if (!content)
        {
            return;
        }

        // The QDockWidget itself can never be shown as - or inside - the central widget: it was
        // briefly tried directly (viewPaneHost->setCentralWidget(m_paneDockWidget)) and crashed as
        // soon as it received a show event, because AzQtComponents::FancyDocking installs a global
        // event filter (FancyDocking::UpdateTitleBars) that assumes every QDockWidget is either
        // normally docked or hosted inside a genuine floating window, and null-derefs otherwise.
        // So instead: pull the pane's plain content widget out and use *that* as the central
        // widget - FancyDocking only reacts to QDockWidgets, not plain QWidgets, so this sidesteps
        // it entirely. The now-empty m_paneDockWidget is detached from the normal dock-area layout
        // and hidden (not deleted) purely so QtViewPaneManager's bookkeeping stays valid for the
        // later CloseViewPane/UnregisterViewPane calls in Deactivate(), which restores content
        // back into it first.
        viewPaneHost->removeDockWidget(m_paneDockWidget);
        m_paneDockWidget->hide();
        m_paneDockWidget->setWidget(nullptr);

        // Deletes the old CLayoutWnd (and the EditorViewportWidget inside it) as a side effect of
        // becoming the new central widget.
        viewPaneHost->setCentralWidget(content);
        content->show();
    }

} // namespace Hammer
