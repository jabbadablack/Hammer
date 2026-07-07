#include "HammerWidget.h"
#include "HammerViewportDisplayController.h"

#include <QDockWidget>
#include <QEvent>
#include <QVBoxLayout>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/CameraInput.h>
#include <AzFramework/Viewport/ViewportControllerList.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
#include <Editor/EditorViewportSettings.h>
#include <Editor/ViewportManipulatorController.h>
#include <Hammer/HammerEditorViewportBus.h>

namespace Hammer
{
    namespace ViewInt = AzToolsFramework::ViewportInteraction;

    AzFramework::WindowSize HammerRenderViewportWidget::GetClientAreaSize() const
    {
        AzFramework::WindowSize size = RenderViewportWidget::GetClientAreaSize();
        size = { AZStd::max(size.m_width, 32u), AZStd::max(size.m_height, 32u) };
        AZ_Assert(size.m_width >= 32 && size.m_height >= 32, "GetClientAreaSize produced a size below the swapchain-safe minimum");
        return size;
    }

    AzFramework::WindowSize HammerRenderViewportWidget::GetRenderResolution() const
    {
        AzFramework::WindowSize size = RenderViewportWidget::GetRenderResolution();
        size = { AZStd::max(size.m_width, 32u), AZStd::max(size.m_height, 32u) };
        AZ_Assert(size.m_width >= 32 && size.m_height >= 32, "GetRenderResolution produced a size below the swapchain-safe minimum");
        return size;
    }

    namespace
    {
        AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget& root)
        {
            const QList<QWidget*> children = root.findChildren<QWidget*>();
            AZ_Assert(!children.isEmpty(), "FindRealRenderViewport given a widget with no child widgets");
            const auto it = AZStd::find_if(
                children.begin(), children.end(),
                [](QWidget* child)
                {
                    return dynamic_cast<AtomToolsFramework::RenderViewportWidget*>(child) != nullptr;
                });
            auto* renderViewport =
                it != children.end() ? dynamic_cast<AtomToolsFramework::RenderViewportWidget*>(*it) : nullptr;
            AZ_Error("HammerWidget", renderViewport, "No RenderViewportWidget was found under the adopted editor viewport");
            renderViewport && (renderViewport->SetInputProcessingEnabled(false), true);
            return renderViewport;
        }

        void SetNamedPassEnabled(AZ::RPI::RenderPipeline& pipeline, const char* passName, bool enabled)
        {
            AZ_Assert(passName && passName[0], "SetNamedPassEnabled called with an empty pass name");
            AZ_Assert(!pipeline.GetId().IsEmpty(), "SetNamedPassEnabled given a pipeline with no id");
            AZ::RPI::Ptr<AZ::RPI::Pass> pass = pipeline.FindFirstPass(AZ::Name(passName));
            pass && (pass->SetEnabled(enabled), true);
        }

        void SetActivePassesEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled)
        {
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);
            AZ_Warning(
                "HammerWidget", !viewportContext || pipeline,
                "Viewport context '%s' has no current pipeline; active-state passes were not updated",
                viewportContext ? viewportContext->GetName().GetCStr() : "");

            AZ::RPI::Ptr<AZ::RPI::Pass> twoDPass;
            pipeline && (twoDPass = pipeline->FindFirstPass(AZ::Name("2DPass")), true);

            AZ_Error(
                "HammerWidget", !pipeline || twoDPass,
                "Could not find this viewport's own \"2DPass\" - entity icons may keep rendering in inactive viewports");

            twoDPass && (twoDPass->SetEnabled(enabled), true);
            pipeline &&
                (SetNamedPassEnabled(*pipeline, "Shadows", enabled),
                 SetNamedPassEnabled(*pipeline, "MotionVectorPass", enabled),
                 SetNamedPassEnabled(*pipeline, "RayTracingAccelerationStructurePass", enabled), true);
        }

        void SyncViewportContextName(AZ::RPI::ViewportContextPtr viewportContext, bool active, bool isAdoptedViewport)
        {
            AZ_Assert(viewportContext, "SyncViewportContextName called with a null ViewportContext");

            auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
            AZ_Error("HammerWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");

            const AZ::Name defaultName = viewportContextManager->GetDefaultViewportContextName();

            (isAdoptedViewport && active && viewportContext->GetName() != defaultName) &&
                (viewportContextManager->RenameViewportContext(viewportContext, defaultName), true);
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
            AZ_Warning(
                "HammerWidget", !viewportContext || pipeline,
                "Viewport context '%s' has no current pipeline; render tick state was not applied",
                viewportContext ? viewportContext->GetName().GetCStr() : "");
            pipeline && (RenderTickActions[static_cast<size_t>(enabled)](*pipeline), true);
        }

        void SetViewModePassesEnabled(AZ::RPI::ViewportContextPtr viewportContext, const HammerViewModes& viewModes)
        {
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);
            AZ_Warning(
                "HammerWidget", !viewportContext || pipeline,
                "Viewport context '%s' has no current pipeline; view-mode passes were not updated",
                viewportContext ? viewportContext->GetName().GetCStr() : "");

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

        class HammerCameraViewportContext final
            : public AtomToolsFramework::ModularCameraViewportContext
        {
        public:
            explicit HammerCameraViewportContext(AzFramework::ViewportId viewportId)
                : m_viewportId(viewportId)
            {
                AZ_Assert(viewportId != AzFramework::InvalidViewportId, "HammerCameraViewportContext created with an invalid ViewportId");
            }

            AZ::Transform GetCameraTransform() const override
            {
                AZ_Assert(m_viewportId != AzFramework::InvalidViewportId, "GetCameraTransform called with an invalid ViewportId");
                AZ::RPI::ViewportContextPtr viewportContext = ResolveViewportContext();
                return viewportContext ? viewportContext->GetCameraTransform() : AZ::Transform::CreateIdentity();
            }

            void SetCameraTransform(const AZ::Transform& transform) override
            {
                AZ_Assert(m_viewportId != AzFramework::InvalidViewportId, "SetCameraTransform called with an invalid ViewportId");
                AZ_Assert(transform.IsOrthogonal(), "SetCameraTransform given a non-orthogonal camera transform");
                AZ::RPI::ViewportContextPtr viewportContext = ResolveViewportContext();
                viewportContext && (viewportContext->SetCameraTransform(transform), true);
            }

            void ConnectViewMatrixChangedHandler(AZ::RPI::MatrixChangedEvent::Handler& handler) override
            {
                AZ::RPI::ViewportContextPtr viewportContext = ResolveViewportContext();
                AZ_Error(
                    "HammerWidget", viewportContext,
                    "Could not resolve viewport context %d to connect the view matrix handler", m_viewportId);
                viewportContext && (viewportContext->ConnectViewMatrixChangedHandler(handler), true);
            }

        private:
            AZ::RPI::ViewportContextPtr ResolveViewportContext() const
            {
                auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
                AZ_Assert(viewportContextManager, "AZ::RPI::ViewportContextRequestsInterface is unavailable");
                return viewportContextManager ? viewportContextManager->GetViewportContextById(m_viewportId) : nullptr;
            }

            AzFramework::ViewportId m_viewportId;
        };

        AzFramework::ViewportControllerPtr BuildViewportCameraController(AzFramework::ViewportId viewportId)
        {
            AZ_Assert(viewportId != AzFramework::InvalidViewportId, "BuildViewportCameraController called with an invalid ViewportId");

            AzFramework::TranslateCameraInputChannelIds translateIds;
            translateIds.m_forwardChannelId = SandboxEditor::CameraTranslateForwardChannelId();
            translateIds.m_backwardChannelId = SandboxEditor::CameraTranslateBackwardChannelId();
            translateIds.m_leftChannelId = SandboxEditor::CameraTranslateLeftChannelId();
            translateIds.m_rightChannelId = SandboxEditor::CameraTranslateRightChannelId();
            translateIds.m_upChannelId = SandboxEditor::CameraTranslateUpChannelId();
            translateIds.m_downChannelId = SandboxEditor::CameraTranslateDownChannelId();
            translateIds.m_boostChannelId = SandboxEditor::CameraTranslateBoostChannelId();

            auto rotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(SandboxEditor::CameraFreeLookChannelId());
            rotateCamera->m_rotateSpeedFn = &SandboxEditor::CameraRotateSpeed;
            rotateCamera->SetActivationBeganFn(
                [viewportId]
                {
                    SandboxEditor::CameraCaptureCursorForLook() &&
                        (ViewInt::ViewportMouseCursorRequestBus::Event(
                             viewportId, &ViewInt::ViewportMouseCursorRequestBus::Events::BeginCursorCapture),
                         true);
                });
            rotateCamera->SetActivationEndedFn(
                [viewportId]
                {
                    SandboxEditor::CameraCaptureCursorForLook() &&
                        (ViewInt::ViewportMouseCursorRequestBus::Event(
                             viewportId, &ViewInt::ViewportMouseCursorRequestBus::Events::EndCursorCapture),
                         true);
                });

            auto translateCamera = AZStd::make_shared<AzFramework::TranslateCameraInput>(
                translateIds, AzFramework::LookTranslation, AzFramework::TranslatePivotLook);
            translateCamera->m_translateSpeedFn = &SandboxEditor::CameraTranslateSpeedScaled;
            translateCamera->m_boostMultiplierFn = &SandboxEditor::CameraBoostMultiplier;

            auto scrollCamera = AZStd::make_shared<AzFramework::LookScrollTranslationCameraInput>();
            scrollCamera->m_scrollSpeedFn = &SandboxEditor::CameraScrollSpeedScaled;

            AZ_Assert(
                rotateCamera && translateCamera && scrollCamera,
                "BuildViewportCameraController failed to allocate its camera inputs");

            auto orbitCamera = AZStd::make_shared<AzFramework::OrbitCameraInput>(SandboxEditor::CameraOrbitChannelId());
            orbitCamera->SetPivotFn(
                [](const AZ::Vector3& position, const AZ::Vector3& direction)
                {
                    return position + direction * SandboxEditor::CameraDefaultOrbitDistance();
                });

            auto orbitRotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(SandboxEditor::CameraOrbitLookChannelId());
            orbitRotateCamera->m_rotateSpeedFn = rotateCamera->m_rotateSpeedFn;

            auto orbitTranslateCamera = AZStd::make_shared<AzFramework::TranslateCameraInput>(
                translateIds, AzFramework::LookTranslation, AzFramework::TranslateOffsetOrbit);
            orbitTranslateCamera->m_translateSpeedFn = translateCamera->m_translateSpeedFn;
            orbitTranslateCamera->m_boostMultiplierFn = translateCamera->m_boostMultiplierFn;

            auto orbitScrollDollyCamera = AZStd::make_shared<AzFramework::OrbitScrollDollyCameraInput>();
            orbitScrollDollyCamera->m_scrollSpeedFn = scrollCamera->m_scrollSpeedFn;

            AZ_Assert(
                orbitCamera && orbitRotateCamera && orbitTranslateCamera && orbitScrollDollyCamera,
                "BuildViewportCameraController failed to allocate its orbit camera inputs");

            orbitCamera->m_orbitCameras.AddCamera(orbitRotateCamera);
            orbitCamera->m_orbitCameras.AddCamera(orbitTranslateCamera);
            orbitCamera->m_orbitCameras.AddCamera(orbitScrollDollyCamera);

            auto controller = AZStd::make_shared<AtomToolsFramework::ModularViewportCameraController>();
            controller->SetCameraViewportContextBuilderCallback(
                [viewportId](AZStd::unique_ptr<AtomToolsFramework::ModularCameraViewportContext>& cameraViewportContext)
                {
                    cameraViewportContext = AZStd::make_unique<HammerCameraViewportContext>(viewportId);
                });
            controller->SetCameraPriorityBuilderCallback(
                [](AtomToolsFramework::CameraControllerPriorityFn& priorityFn)
                {
                    priorityFn = AtomToolsFramework::DefaultCameraControllerPriority;
                });
            controller->SetCameraPropsBuilderCallback(
                [](AzFramework::CameraProps& cameraProps)
                {
                    cameraProps.m_rotateSmoothnessFn = &SandboxEditor::CameraRotateSmoothness;
                    cameraProps.m_translateSmoothnessFn = &SandboxEditor::CameraTranslateSmoothness;
                    cameraProps.m_rotateSmoothingEnabledFn = &SandboxEditor::CameraRotateSmoothingEnabled;
                    cameraProps.m_translateSmoothingEnabledFn = &SandboxEditor::CameraTranslateSmoothingEnabled;
                });
            controller->SetCameraListBuilderCallback(
                [rotateCamera, translateCamera, scrollCamera, orbitCamera](AzFramework::Cameras& cameras)
                {
                    cameras.AddCamera(rotateCamera);
                    cameras.AddCamera(translateCamera);
                    cameras.AddCamera(scrollCamera);
                    cameras.AddCamera(orbitCamera);
                });

            AZ_Assert(controller, "BuildViewportCameraController failed to allocate the camera controller");
            return controller;
        }
    } // namespace

    HammerWidget::HammerWidget(QWidget* parent)
        : QWidget(parent)
    {
        AZ_Assert(parent, "HammerWidget constructed without a parent");
        AZ_Assert(qobject_cast<QDockWidget*>(parent), "HammerWidget expects to be hosted in a QDockWidget");

        setMinimumSize(32, 32);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_viewportWidget = new HammerRenderViewportWidget(this, false);
        m_viewportWidget->setMinimumSize(32, 32);
        m_viewportWidget->installEventFilter(this);
        layout->addWidget(m_viewportWidget);
    }

    HammerWidget::HammerWidget(QWidget* parent, QWidget& realViewport, AdoptTag)
        : QWidget(parent)
    {
        AZ_Assert(qobject_cast<QDockWidget*>(parent), "Adopting HammerWidget expects to be hosted in a QDockWidget");

        setMinimumSize(32, 32);
        m_adoptedRealViewport = &realViewport;
        realViewport.hide();
        realViewport.setParent(nullptr);
        realViewport.setParent(this);
        realViewport.setMinimumSize(32, 32);

        m_viewportWidget = FindRealRenderViewport(realViewport);
        AZ_Error("HammerWidget", m_viewportWidget, "Could not resolve the adopted real viewport's inner RenderViewportWidget");
        m_viewportWidget &&
            (m_viewportWidget->installEventFilter(this),
             m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerSelectionCacheController>()), true);

        SyncAdoptedGeometry();

        m_sceneInitialized = true;

        ApplyActiveState();
        ApplyRenderTickState();
    }

    HammerWidget::~HammerWidget()
    {
        AZ_Assert(!m_cameraController || m_sceneInitialized, "A camera controller exists on a viewport whose scene was never initialized");

        m_adoptedRealViewport && (m_adoptedRealViewport->setParent(nullptr), true);

        AZ::RPI::ViewportContextPtr ownedContext =
            (!m_adoptedRealViewport && m_sceneInitialized && m_viewportWidget) ? m_viewportWidget->GetViewportContext() : nullptr;
        AZ::RPI::RenderPipelinePtr ownedPipeline = ownedContext ? ownedContext->GetCurrentPipeline() : nullptr;
        ownedPipeline && (ownedPipeline->RemoveFromRenderTick(), ownedPipeline->RemoveFromScene(), true);

        const bool neverInitialized = !m_sceneInitialized && m_viewportWidget;
        AZ_Warning(
            "HammerWidget", !neverInitialized || AZ::RHI::RHISystemInterface::Get(),
            "A Hammer viewport was destroyed before ever being shown, and the RHI system has already shut down; skipping "
            "deferred initialization (a known AtomToolsFramework::RenderViewportWidget shutdown issue may occur)");

        (neverInitialized && AZ::RHI::RHISystemInterface::Get()) && (m_viewportWidget->InitializeViewportContext(), true);
    }

    HammerWidget* HammerWidget::CreateAdopting(QWidget* parent, QWidget& realViewport)
    {
        AZ_Assert(parent, "CreateAdopting called with a null parent");
        AZ_Assert(&realViewport != parent, "CreateAdopting given the parent dock as the viewport to adopt");
        return new HammerWidget(parent, realViewport, AdoptTag{});
    }

    void HammerWidget::SyncAdoptedGeometry()
    {
        AZ_Assert(m_adoptedRealViewport || m_viewportWidget, "SyncAdoptedGeometry called with no viewport widgets to position");
        m_adoptedRealViewport && (m_adoptedRealViewport->setGeometry(rect()), true);
        m_viewportWidget && (m_viewportWidget->setGeometry(QRect(QPoint(0, 0), rect().size())), true);
    }

    void HammerWidget::resizeEvent(QResizeEvent* event)
    {
        AZ_Assert(event, "resizeEvent invoked with a null event");
        QWidget::resizeEvent(event);
        m_adoptedRealViewport && (SyncAdoptedGeometry(), true);
        InitializeSceneIfReady();
    }

    void HammerWidget::showEvent(QShowEvent* event)
    {
        AZ_Assert(event, "showEvent invoked with a null event");
        QWidget::showEvent(event);
        m_adoptedRealViewport && (SyncAdoptedGeometry(), true);
        InitializeSceneIfReady();
    }

    bool HammerWidget::eventFilter(QObject* watched, QEvent* event)
    {
        AZ_Assert(watched && event, "eventFilter invoked with a null object or event");
        (watched == m_viewportWidget && event->type() == QEvent::FocusIn) && (emit ViewportFocusRequested(), true);
        return QWidget::eventFilter(watched, event);
    }

    void HammerWidget::SetActive(bool active)
    {
        AZ_Assert(m_viewportWidget || m_adoptedRealViewport, "SetActive called on a HammerWidget with no render viewport");
        m_active = active;
        (m_sceneInitialized && m_viewportWidget) && (ApplyActiveState(), true);
    }

    void HammerWidget::SetRenderTickEnabled(bool enabled)
    {
        AZ_Assert(m_viewportWidget || m_adoptedRealViewport, "SetRenderTickEnabled called on a HammerWidget with no render viewport");
        m_renderTickEnabled = enabled;
        (m_sceneInitialized && m_viewportWidget) && (ApplyRenderTickState(), true);
    }

    void HammerWidget::SetViewModes(const HammerViewModes& viewModes)
    {
        AZ_Assert(m_viewportWidget || m_adoptedRealViewport, "SetViewModes called on a HammerWidget with no render viewport");
        m_viewModes = viewModes;
        (m_sceneInitialized && m_viewportWidget) && (ApplyViewModes(), true);
    }

    HammerViewModes HammerWidget::GetViewModes() const
    {
        AZ_Assert(m_viewportWidget || m_adoptedRealViewport, "GetViewModes called on a HammerWidget with no render viewport");
        return m_viewModes;
    }

    void HammerWidget::SetGameModeSuppressed(bool suppressed)
    {
        AZ_Assert(m_viewportWidget || m_adoptedRealViewport, "SetGameModeSuppressed called on a HammerWidget with no render viewport");
        m_viewModesSuppressed = suppressed;
        (m_sceneInitialized && m_viewportWidget) && (ApplyViewModes(), ApplyRenderTickState(), true);
    }

    void HammerWidget::ApplyViewModes()
    {
        AZ_Assert(m_viewportWidget, "ApplyViewModes called without an initialized viewport widget");
        AZ_Assert(m_sceneInitialized, "ApplyViewModes called before the scene was initialized");
        SetViewModePassesEnabled(
            m_viewportWidget->GetViewportContext(), m_viewModesSuppressed ? HammerViewModes{} : m_viewModes);
    }

    void HammerWidget::ApplyActiveState()
    {
        AZ_Assert(m_viewportWidget, "ApplyActiveState called without an initialized viewport widget");
        AZ_Assert(m_sceneInitialized, "ApplyActiveState called before the scene was initialized");

        m_viewportWidget->SetInputProcessingEnabled(m_active);
        ApplyRenderTickState();

        m_active &&
            (HammerEditorActiveViewportRequestBus::Broadcast(
                 &HammerEditorActiveViewportRequests::SetActiveViewportId, m_viewportWidget->GetId()),
             true);

        AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext();
        AZ_Assert(viewportContext, "ApplyActiveState found no ViewportContext on an initialized viewport");
        (viewportContext && !m_pipelineChangedHandler.IsConnected()) &&
            (m_pipelineChangedHandler = AZ::RPI::ViewportContext::PipelineChangedEvent::Handler(
                 [this](AZ::RPI::RenderPipelinePtr)
                 {
                     SetActivePassesEnabled(m_viewportWidget->GetViewportContext(), m_active);
                     ApplyViewModes();
                 }),
             viewportContext->ConnectCurrentPipelineChangedHandler(m_pipelineChangedHandler),
             true);
        SetActivePassesEnabled(viewportContext, m_active);
        SyncViewportContextName(viewportContext, m_active, m_adoptedRealViewport != nullptr);
    }

    void HammerWidget::InitializeSceneIfReady()
    {
        AZ_Assert(m_sceneInitialized || !m_adoptedRealViewport, "Adopting viewports must initialize in their constructor");
        const bool shouldInitialize =
            !m_sceneInitialized && m_viewportWidget && m_viewportWidget->width() >= 1 && m_viewportWidget->height() >= 1;

        shouldInitialize && (InitializeScene(), true);
    }

    void HammerWidget::InitializeScene()
    {
        AZ_Assert(!m_sceneInitialized, "InitializeScene called after the scene was already initialized");
        AZ_Assert(m_viewportWidget, "InitializeScene called without an allocated viewport widget");

        m_viewportWidget->InitializeViewportContext();
        m_sceneInitialized = true;
        AZ_Assert(m_viewportWidget->GetViewportContext(), "InitializeViewportContext did not produce a viewport context");

        AzFramework::ISceneSystem* sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Error("HammerWidget", sceneSystem, "AzFramework::SceneSystemInterface is not available; Hammer cannot bind a scene");

        AZStd::shared_ptr<AzFramework::Scene> mainScene;
        sceneSystem && (mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName), true);
        AZ_Error("HammerWidget", mainScene, "Main scene '%s' not found", AzFramework::Scene::MainSceneName.data());
        mainScene && (m_viewportWidget->SetScene(mainScene, true), true);

        SetupCamera();
        AZ_Assert(m_viewportWidget->GetControllerList(), "The initialized viewport has no controller list");
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerSelectionCacheController>());
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<SandboxEditor::ViewportManipulatorController>());
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportDisplayController>());

        ApplyActiveState();
        ApplyRenderTickState();
        ApplyViewModes();
    }

    void HammerWidget::SetupCamera()
    {
        AZ_Assert(m_viewportWidget, "SetupCamera called without an allocated viewport widget");
        AZ_Assert(m_viewportWidget->GetId() != AzFramework::InvalidViewportId, "SetupCamera called before the viewport context was initialized");
        AZ_Assert(!m_cameraController, "SetupCamera called while a camera controller already exists");

        m_cameraController = BuildViewportCameraController(m_viewportWidget->GetId());
        m_viewportWidget->GetControllerList()->Add(m_cameraController);

        AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext();
        AZ_Assert(viewportContext, "SetupCamera called before the viewport context was initialized");
        const auto applyProjection = [this](AzFramework::WindowSize size)
        {
            AZ::Matrix4x4 viewToClip;
            AZ::MakePerspectiveFovMatrixRH(
                viewToClip, SandboxEditor::CameraDefaultFovRadians(),
                aznumeric_cast<float>(AZStd::max(size.m_width, 1u)) / aznumeric_cast<float>(AZStd::max(size.m_height, 1u)),
                SandboxEditor::CameraDefaultNearPlaneDistance(), SandboxEditor::CameraDefaultFarPlaneDistance(), true);
            m_viewportWidget->GetViewportContext()->SetCameraProjectionMatrix(viewToClip);
        };
        m_viewportSizeChangedHandler = AZ::RPI::ViewportContext::SizeChangedEvent::Handler(applyProjection);
        viewportContext &&
            (viewportContext->ConnectSizeChangedHandler(m_viewportSizeChangedHandler),
             applyProjection(viewportContext->GetViewportSize()), true);
    }

    void HammerWidget::ApplyRenderTickState()
    {
        AZ_Assert(m_viewportWidget, "ApplyRenderTickState called without an initialized viewport widget");
        AZ_Assert(m_sceneInitialized, "ApplyRenderTickState called before the scene was initialized");
        SetPipelineRenderTickEnabled(m_viewportWidget->GetViewportContext(), m_renderTickEnabled);
        (!m_adoptedRealViewport) &&
            (m_viewportWidget->GetControllerList()->SetEnabled(m_renderTickEnabled && !m_viewModesSuppressed), true);
    }

    AzFramework::ViewportId HammerWidget::GetViewportId() const
    {
        AZ_Assert(m_viewportWidget || m_adoptedRealViewport, "GetViewportId called on a HammerWidget with no render viewport");
        return (m_viewportWidget && m_sceneInitialized) ? m_viewportWidget->GetId() : AzFramework::InvalidViewportId;
    }
}
