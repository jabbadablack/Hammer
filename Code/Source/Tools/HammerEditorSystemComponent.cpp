
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Settings/SettingsRegistry.h>

#include <Atom/RPI.Public/FeatureProcessorFactory.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/WindowContext.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QSettings>

#include "HammerEditorSystemComponent.h"
#include "HammerWireframeFeatureProcessor.h"

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
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
        AZ::RPI::ViewportContextManagerNotificationsBus::Handler::BusConnect();

        AZ::RPI::FeatureProcessorFactory::Get()->RegisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        m_loadTemplatesHandler = AZ::RPI::PassSystemInterface::OnReadyLoadTemplatesEvent::Handler(
            []()
            {
                AZ::RPI::PassSystemInterface::Get()->LoadPassTemplateMappings("Passes/HammerWireframePasses.azasset");
            });
        AZ::RPI::PassSystemInterface::Get()->ConnectEvent(m_loadTemplatesHandler);

        // Must happen before MainWindow (and therefore CLayoutWnd) is constructed, since
        // CLayoutWnd::LoadConfig() only reads this once during MainWindow::InitCentralWidget().
        // Activate() runs well before that (system components activate before the Editor's Qt
        // UI is built), so this is early enough.
        ForceSplitViewportLayout();
    }

    void HammerEditorSystemComponent::Deactivate()
    {
        m_pipelineChangedHandler.Disconnect();
        if (m_wireframePipeline)
        {
            m_wireframePipeline->RemoveFromScene();
            m_wireframePipeline = nullptr;
        }

        m_loadTemplatesHandler.Disconnect();
        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<HammerWireframeFeatureProcessor>();

        AZ::RPI::ViewportContextManagerNotificationsBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        HammerSystemComponent::Deactivate();
    }

    void HammerEditorSystemComponent::NotifyRegisterViews()
    {
        // Hammer no longer registers a dockable view pane of its own - see the class comment in
        // the header for why: it now claims the Editor's own second native split-viewport pane
        // instead of trying to dock a separate widget alongside it.
    }

    void HammerEditorSystemComponent::ForceSplitViewportLayout()
    {
        // CLayoutWnd::CreateLayout() (Code/Editor/LayoutWnd.cpp) silently forces single-viewport
        // (ET_Layout0) regardless of the requested layout unless this settings registry key is
        // true. Shipped as Registry/hammer_viewport_settings.setreg so it's set with no engine
        // source changes and no runtime code needed for this half of the toggle.
        if (auto* registry = AZ::SettingsRegistry::Get())
        {
            bool multiViewportEnabled = false;
            registry->Get(multiViewportEnabled, "/O3DE/Viewport/MultiViewportEnabled");
            AZ_Warning(
                "HammerEditorSystemComponent", multiViewportEnabled,
                "/O3DE/Viewport/MultiViewportEnabled is not set; Hammer's split viewport will not appear. "
                "Check Registry/hammer_viewport_settings.setreg is being picked up.");
        }

        // The other half: which layout to actually use. This is persisted via QSettings (plain
        // Windows registry, not the AZ settings registry) under the same key CLayoutWnd itself
        // reads/writes from SaveConfig()/LoadConfig() - group "ViewportLayout", value "Layout".
        // 1 == ET_Layout1, the Editor's built-in "2 Viewports" side-by-side split.
        QSettings settings;
        settings.beginGroup("ViewportLayout");
        settings.setValue("Layout", 1);
        settings.endGroup();
    }

    void HammerEditorSystemComponent::OnViewportContextAdded(AZ::RPI::ViewportContextPtr viewportContext)
    {
        if (!viewportContext || m_takenOverViewportId != AzFramework::InvalidViewportId)
        {
            return;
        }

        const AZ::Name defaultName = AZ::RPI::ViewportContextRequests::Get()->GetDefaultViewportContextName();
        if (viewportContext->GetName() == defaultName)
        {
            // The main viewport - never touch this one.
            return;
        }

        m_takenOverViewportId = viewportContext->GetId();

        if (viewportContext->GetCurrentPipeline())
        {
            // The pane already has a default pipeline bound (created synchronously as part of
            // its own viewport widget construction) - swap it out immediately.
            SwapInWireframePipeline(viewportContext);
        }
        else
        {
            // Otherwise wait for its first pipeline to be assigned, then swap that one out. This
            // handler disconnects itself inside SwapInWireframePipeline so it doesn't react to
            // our own AddRenderPipeline call below.
            m_pipelineChangedHandler = AZ::Event<AZ::RPI::RenderPipelinePtr>::Handler(
                [this, viewportContext]([[maybe_unused]] AZ::RPI::RenderPipelinePtr newPipeline)
                {
                    SwapInWireframePipeline(viewportContext);
                });
            viewportContext->ConnectCurrentPipelineChangedHandler(m_pipelineChangedHandler);
        }
    }

    void HammerEditorSystemComponent::OnViewportContextRemoved(AzFramework::ViewportId viewportId)
    {
        if (viewportId == m_takenOverViewportId)
        {
            m_pipelineChangedHandler.Disconnect();
            // The ViewportContext (and its WindowContext/swapchain) is already gone at this
            // point, so the pipeline can't be meaningfully removed from a scene anymore - just
            // drop our reference.
            m_wireframePipeline = nullptr;
            m_takenOverViewportId = AzFramework::InvalidViewportId;
        }
    }

    void HammerEditorSystemComponent::SwapInWireframePipeline(AZ::RPI::ViewportContextPtr viewportContext)
    {
        m_pipelineChangedHandler.Disconnect();

        if (AZ::RPI::RenderPipelinePtr existingPipeline = viewportContext->GetCurrentPipeline())
        {
            existingPipeline->RemoveFromScene();
        }

        AZ::RPI::ScenePtr scene = viewportContext->GetRenderScene();
        AZ::RPI::WindowContextSharedPtr windowContext = viewportContext->GetWindowContext();
        AZ_Error(
            "HammerEditorSystemComponent", scene && windowContext,
            "Claimed ViewportContext %u is missing a scene or window context", viewportContext->GetId());
        if (!scene || !windowContext)
        {
            return;
        }

        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
        pipelineDesc.m_name = "HammerWireframePipeline";
        pipelineDesc.m_rootPassTemplate = "HammerWireframePipelineTemplate";

        AZ::RPI::RenderPipelinePtr pipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *windowContext);
        AZ_Error(
            "HammerEditorSystemComponent", pipeline,
            "Failed to create HammerWireframePipeline; is HammerWireframePipelineTemplate registered/compiled?");
        if (!pipeline)
        {
            return;
        }

        scene->AddRenderPipeline(pipeline);
        m_wireframePipeline = pipeline;
    }

} // namespace Hammer
