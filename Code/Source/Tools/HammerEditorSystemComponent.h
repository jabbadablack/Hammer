
#pragma once

#include <Clients/HammerSystemComponent.h>

#include <Atom/RPI.Public/Base.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

namespace Hammer
{
    /// System component for Hammer editor
    class HammerEditorSystemComponent
        : public HammerSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
        , private AZ::RPI::ViewportContextManagerNotificationsBus::Handler
    {
        using BaseSystemComponent = HammerSystemComponent;
    public:
        AZ_COMPONENT_DECL(HammerEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        HammerEditorSystemComponent();
        ~HammerEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::EditorEventsBus overrides ...
        void NotifyRegisterViews() override;

        // ViewportContextManagerNotificationsBus overrides ...
        // Hammer no longer opens its own dockable window. Instead it forces the Editor into its
        // native, built-in "2 Viewports" split (see ForceSplitViewportLayout) - a real,
        // already-anchored QSplitter pane the Editor itself manages, with no FancyDocking
        // involved at all - and claims whichever extra AZ::RPI::ViewportContext that split
        // creates for the second pane by swapping its render pipeline to Hammer's wireframe one.
        void OnViewportContextAdded(AZ::RPI::ViewportContextPtr viewportContext) override;
        void OnViewportContextRemoved(AzFramework::ViewportId viewportId) override;

        // Persists "Layout" = ET_Layout1 (2 viewports side-by-side) via QSettings, the same
        // storage CLayoutWnd::LoadConfig/SaveConfig (Code/Editor/LayoutWnd.cpp) uses, so the
        // Editor boots straight into the split layout. Requires the
        // "/O3DE/Viewport/MultiViewportEnabled" settings registry key (see
        // Registry/hammer_viewport_settings.setreg) to be true, or CLayoutWnd silently forces
        // single-viewport regardless of this value.
        void ForceSplitViewportLayout();

        // Removes whatever render pipeline is currently bound to viewportContext's window and
        // replaces it with a freshly-built HammerWireframePipeline instance.
        void SwapInWireframePipeline(AZ::RPI::ViewportContextPtr viewportContext);

        AZ::RPI::PassSystemInterface::OnReadyLoadTemplatesEvent::Handler m_loadTemplatesHandler;
        AZ::Event<AZ::RPI::RenderPipelinePtr>::Handler m_pipelineChangedHandler;
        AZ::RPI::RenderPipelinePtr m_wireframePipeline;
        AzFramework::ViewportId m_takenOverViewportId = AzFramework::InvalidViewportId;
    };
} // namespace Hammer
