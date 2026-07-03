#include "HammerAtomRenderBackendAdapter.h"
#include "HammerRenderViewportWidget.h"
#include "HammerViewportCameraFactory.h"

#include "../HammerNames.h"

#include <AzCore/std/containers/array.h>

#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>

namespace Hammer
{
    namespace
    {
        void AddToRenderTick(AZ::RPI::RenderPipeline& pipeline)
        {
            pipeline.AddToRenderTick();
        }

        void RemoveFromRenderTick(AZ::RPI::RenderPipeline& pipeline)
        {
            pipeline.RemoveFromRenderTick();
        }

        using RenderTickAction = void (*)(AZ::RPI::RenderPipeline&);
        constexpr AZStd::array<RenderTickAction, 2> RenderTickActions = { &RemoveFromRenderTick, &AddToRenderTick };
    } // namespace

    HammerAtomRenderBackendAdapter::HammerAtomRenderBackendAdapter(AZ::RPI::ViewportContextRequestsInterface& viewportContextManager)
        : m_viewportContextManager(viewportContextManager)
    {
    }

    void HammerAtomRenderBackendAdapter::RenameDefaultViewportContext(const AZ::Name& newName) const
    {
        AZ::RPI::ViewportContextPtr defaultContext = m_viewportContextManager.GetDefaultViewportContext();
        AZ_Error("HammerAtomRenderBackendAdapter", defaultContext, "Could not find the default ViewportContext to rename");
        defaultContext && (m_viewportContextManager.RenameViewportContext(defaultContext, newName), true);
    }

    AZ::RPI::ViewportContextPtr HammerAtomRenderBackendAdapter::FindViewportContextByName(const AZ::Name& name) const
    {
        return m_viewportContextManager.GetViewportContextByName(name);
    }

    void HammerAtomRenderBackendAdapter::SyncActiveCamera(
        AZ::RPI::ViewportContextPtr hiddenContext, AZ::RPI::ViewportContextPtr activeContext) const
    {
        (hiddenContext && activeContext) && (hiddenContext->SetCameraTransform(activeContext->GetCameraTransform()), true);

        AZ::RPI::RenderPipelinePtr pipeline;
        hiddenContext && (pipeline = hiddenContext->GetCurrentPipeline(), true);
        pipeline && (pipeline->RemoveFromRenderTick(), true);
    }

    void HammerAtomRenderBackendAdapter::SetOverlayPassEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled) const
    {
        AZ::RPI::RenderPipelinePtr pipeline;
        viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);

        AZ::RPI::Ptr<AZ::RPI::Pass> twoDPass;
        pipeline && (twoDPass = pipeline->FindFirstPass(AZ::Name("2DPass")), true);
        AZ_Error(
            "HammerAtomRenderBackendAdapter", twoDPass,
            "Could not find this viewport's own \"2DPass\" - entity icons may keep rendering in inactive viewports");
        twoDPass && (twoDPass->SetEnabled(enabled), true);
    }

    void HammerAtomRenderBackendAdapter::SetRenderTickEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled) const
    {
        AZ::RPI::RenderPipelinePtr pipeline;
        viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);
        pipeline && (RenderTickActions[static_cast<size_t>(enabled)](*pipeline), true);
    }

    void HammerAtomRenderBackendAdapter::SyncViewportContextName(
        AZ::RPI::ViewportContextPtr viewportContext, AzFramework::ViewportId viewportId, bool active) const
    {
        AZ_Assert(viewportContext, "SyncViewportContextName called with a null ViewportContext");

        const AZ::Name defaultName = m_viewportContextManager.GetDefaultViewportContextName();
        const AZ::Name currentName = viewportContext->GetName();

        const AZStd::array<AZ::Name, 2> candidateNames = { Names::PerViewportContextName(viewportId), defaultName };
        const AZ::Name& desiredName = candidateNames[static_cast<size_t>(active)];

        (currentName != desiredName) && (m_viewportContextManager.RenameViewportContext(viewportContext, desiredName), true);
    }

    AzFramework::ViewportControllerPtr HammerAtomRenderBackendAdapter::BuildViewportCameraController(
        AzFramework::ViewportId viewportId) const
    {
        return CreateViewportCameraController(viewportId);
    }

    AtomToolsFramework::RenderViewportWidget* HammerAtomRenderBackendAdapter::CreateRenderViewportWidget(QWidget* parent) const
    {
        return new HammerRenderViewportWidget(parent, false);
    }
} // namespace Hammer
