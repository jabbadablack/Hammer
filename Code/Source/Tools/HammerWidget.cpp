#include "HammerWidget.h"

#include <QDockWidget>

#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <Editor/EditorViewportSettings.h>
#include <Editor/EditorViewportWidget.h>

namespace Hammer
{
    namespace
    {
        void SetNamedPassEnabled(AZ::RPI::RenderPipeline& pipeline, const char* passName, bool enabled)
        {
            AZ_Assert(passName && passName[0], "SetNamedPassEnabled called with an empty pass name");
            AZ_Assert(!pipeline.GetId().IsEmpty(), "SetNamedPassEnabled given a pipeline with no id");
            AZ::RPI::Ptr<AZ::RPI::Pass> pass = pipeline.FindFirstPass(AZ::Name(passName));
            pass && (pass->SetEnabled(enabled), true);
        }

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

        void SetPipelineRenderTickEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled)
        {
            AZ_Assert(static_cast<size_t>(enabled) < RenderTickActions.size(), "Render tick action index is out of range");
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);
            pipeline && (RenderTickActions[static_cast<size_t>(enabled)](*pipeline), true);
        }

        void SetViewModePassesEnabled(AZ::RPI::ViewportContextPtr viewportContext, const HammerViewModes& viewModes)
        {
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);
            AZ_Error(
                "HammerWidget", !pipeline || pipeline->FindFirstPass(AZ::Name("HammerWireframePass")),
                "Pipeline '%s' is missing the Hammer view-mode passes", pipeline ? pipeline->GetId().GetCStr() : "");

            const bool flatBackground = !viewModes.m_normal && (viewModes.m_wireframe || viewModes.m_overdraw);
            pipeline &&
                (SetNamedPassEnabled(*pipeline, "OpaquePass", viewModes.m_normal),
                 SetNamedPassEnabled(*pipeline, "Forward", viewModes.m_normal),
                 SetNamedPassEnabled(*pipeline, "TransparentPass", viewModes.m_normal),
                 SetNamedPassEnabled(*pipeline, "HammerViewModeBackgroundPass", flatBackground),
                 SetNamedPassEnabled(*pipeline, "HammerWireframeHiddenPass", viewModes.m_wireframe),
                 SetNamedPassEnabled(*pipeline, "HammerWireframePass", viewModes.m_wireframe),
                 SetNamedPassEnabled(*pipeline, "HammerOverdrawCountPass", viewModes.m_overdraw),
                 SetNamedPassEnabled(*pipeline, "HammerOverdrawResolvePass", viewModes.m_overdraw), true);
        }
    } // namespace

    HammerWidget::HammerWidget(QWidget* parent, EditorViewportWidget& viewport, bool adopted)
        : QWidget(parent)
        , m_engineViewport(&viewport)
        , m_adopted(adopted)
    {
        AZ_Assert(qobject_cast<QDockWidget*>(parent), "HammerWidget expects to be hosted in a QDockWidget");
        AZ_Assert(
            viewport.GetViewportId() != AzFramework::InvalidViewportId,
            "HammerWidget given an EditorViewportWidget whose ViewportId was never assigned");

        setMinimumSize(50, 50);
        viewport.hide();
        viewport.setParent(nullptr);
        viewport.setParent(this);
        viewport.setMinimumSize(50, 50);
        SyncGeometry();
        viewport.show();

        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ::RPI::ViewportContextPtr viewportContext =
            viewportContextManager ? viewportContextManager->GetViewportContextById(GetViewportId()) : nullptr;
        AZ_Error("HammerWidget", viewportContext, "No ViewportContext exists for viewport %d", GetViewportId());

        m_pipelineChangedHandler = AZ::RPI::ViewportContext::PipelineChangedEvent::Handler(
            [this](AZ::RPI::RenderPipelinePtr)
            {
                ApplyViewModes();
            });
        viewportContext && (viewportContext->ConnectCurrentPipelineChangedHandler(m_pipelineChangedHandler), true);

        const auto applyProjection = [this](AzFramework::WindowSize size)
        {
            AZ::Matrix4x4 viewToClip;
            AZ::MakePerspectiveFovMatrixRH(
                viewToClip, SandboxEditor::CameraDefaultFovRadians(),
                aznumeric_cast<float>(AZStd::max(size.m_width, 1u)) / aznumeric_cast<float>(AZStd::max(size.m_height, 1u)),
                SandboxEditor::CameraDefaultNearPlaneDistance(), SandboxEditor::CameraDefaultFarPlaneDistance(), true);
            auto* contextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
            AZ::RPI::ViewportContextPtr context = contextManager ? contextManager->GetViewportContextById(GetViewportId()) : nullptr;
            context && (context->SetCameraProjectionMatrix(viewToClip), true);
        };
        m_viewportSizeChangedHandler = AZ::RPI::ViewportContext::SizeChangedEvent::Handler(applyProjection);
        (!m_adopted && viewportContext) &&
            (viewportContext->ConnectSizeChangedHandler(m_viewportSizeChangedHandler),
             applyProjection(viewportContext->GetViewportSize()), true);

        ApplyRenderTickState();
        ApplyViewModes();
    }

    HammerWidget::~HammerWidget()
    {
        AZ_Assert(m_engineViewport, "HammerWidget destroyed without an engine viewport");
        m_adopted && (m_engineViewport->setParent(nullptr), true);
    }

    EditorViewportWidget* HammerWidget::EngineViewport() const
    {
        return m_engineViewport;
    }

    void HammerWidget::SyncGeometry()
    {
        AZ_Assert(m_engineViewport, "SyncGeometry called without an engine viewport");
        m_engineViewport->setGeometry(rect());
    }

    void HammerWidget::resizeEvent(QResizeEvent* event)
    {
        AZ_Assert(event, "resizeEvent invoked with a null event");
        QWidget::resizeEvent(event);
        SyncGeometry();
    }

    void HammerWidget::showEvent(QShowEvent* event)
    {
        AZ_Assert(event, "showEvent invoked with a null event");
        QWidget::showEvent(event);
        SyncGeometry();
    }

    void HammerWidget::SetRenderTickEnabled(bool enabled)
    {
        m_renderTickEnabled = enabled;
        ApplyRenderTickState();
    }

    void HammerWidget::SetViewModes(const HammerViewModes& viewModes)
    {
        m_viewModes = viewModes;
        ApplyViewModes();
    }

    HammerViewModes HammerWidget::GetViewModes() const
    {
        return m_viewModes;
    }

    void HammerWidget::SetGameModeSuppressed(bool suppressed)
    {
        m_viewModesSuppressed = suppressed;
        ApplyViewModes();
    }

    void HammerWidget::ApplyRenderTickState()
    {
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Assert(viewportContextManager, "ApplyRenderTickState called without the ViewportContextManager");
        SetPipelineRenderTickEnabled(
            viewportContextManager ? viewportContextManager->GetViewportContextById(GetViewportId()) : nullptr,
            m_renderTickEnabled);
    }

    void HammerWidget::ApplyViewModes()
    {
        auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        AZ_Assert(viewportContextManager, "ApplyViewModes called without the ViewportContextManager");
        SetViewModePassesEnabled(
            viewportContextManager ? viewportContextManager->GetViewportContextById(GetViewportId()) : nullptr,
            m_viewModesSuppressed ? HammerViewModes{} : m_viewModes);
    }

    AzFramework::ViewportId HammerWidget::GetViewportId() const
    {
        AZ_Assert(m_engineViewport, "GetViewportId called without an engine viewport");
        return m_engineViewport->GetViewportId();
    }
}
