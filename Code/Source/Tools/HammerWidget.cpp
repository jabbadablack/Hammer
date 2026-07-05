#include "HammerWidget.h"
#include "HammerViewportDisplayController.h"

#include <QEvent>
#include <QVBoxLayout>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/CameraInput.h>
#include <AzFramework/Viewport/ViewportControllerList.h>
#include <AzToolsFramework/API/EditorEntityAPI.h>
#include <AzToolsFramework/API/EntityCompositionRequestBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/Entity/PrefabEditorEntityOwnershipInterface.h>
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
        return { AZStd::max(size.m_width, 32u), AZStd::max(size.m_height, 32u) };
    }

    AzFramework::WindowSize HammerRenderViewportWidget::GetRenderResolution() const
    {
        AzFramework::WindowSize size = RenderViewportWidget::GetRenderResolution();
        return { AZStd::max(size.m_width, 32u), AZStd::max(size.m_height, 32u) };
    }

    namespace
    {
        AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget& root)
        {
            const QList<QWidget*> children = root.findChildren<QWidget*>();
            const auto it = AZStd::find_if(
                children.begin(), children.end(),
                [](QWidget* child)
                {
                    return dynamic_cast<AtomToolsFramework::RenderViewportWidget*>(child) != nullptr;
                });
            auto* renderViewport =
                it != children.end() ? dynamic_cast<AtomToolsFramework::RenderViewportWidget*>(*it) : nullptr;
            renderViewport && (renderViewport->SetInputProcessingEnabled(false), true);
            return renderViewport;
        }

        void SetOverlayPassEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled)
        {
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);

            AZ::RPI::Ptr<AZ::RPI::Pass> twoDPass;
            pipeline && (twoDPass = pipeline->FindFirstPass(AZ::Name("2DPass")), true);

            AZ_Error(
                "HammerWidget", !pipeline || twoDPass,
                "Could not find this viewport's own \"2DPass\" - entity icons may keep rendering in inactive viewports");

            twoDPass && (twoDPass->SetEnabled(enabled), true);
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
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);
            pipeline && (RenderTickActions[static_cast<size_t>(enabled)](*pipeline), true);
        }

        void SetNamedPassEnabled(AZ::RPI::RenderPipeline& pipeline, const char* passName, bool enabled)
        {
            AZ::RPI::Ptr<AZ::RPI::Pass> pass = pipeline.FindFirstPass(AZ::Name(passName));
            pass && (pass->SetEnabled(enabled), true);
        }

        void SetViewModePassesEnabled(AZ::RPI::ViewportContextPtr viewportContext, const HammerViewModes& viewModes)
        {
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);

            AZ_Error(
                "HammerWidget", !pipeline || pipeline->FindFirstPass(AZ::Name("HammerWireframePass")),
                "Pipeline '%s' is missing the Hammer view-mode passes", pipeline ? pipeline->GetId().GetCStr() : "");

            pipeline &&
                (SetNamedPassEnabled(*pipeline, "OpaquePass", viewModes.m_normal),
                 SetNamedPassEnabled(*pipeline, "Forward", viewModes.m_normal),
                 SetNamedPassEnabled(*pipeline, "TransparentPass", viewModes.m_normal),
                 SetNamedPassEnabled(*pipeline, "HammerWireframePass", viewModes.m_wireframe),
                 SetNamedPassEnabled(*pipeline, "HammerOverdrawCountPass", viewModes.m_overdraw),
                 SetNamedPassEnabled(*pipeline, "HammerOverdrawResolvePass", viewModes.m_overdraw), true);
        }

        class HammerCameraViewportContext final
            : public AtomToolsFramework::ModularCameraViewportContext
        {
        public:
            HammerCameraViewportContext(AzFramework::ViewportId viewportId, AZ::EntityId cameraEntityId)
                : m_viewportId(viewportId)
                , m_cameraEntityId(cameraEntityId)
            {
            }

            AZ::Transform GetCameraTransform() const override
            {
                AZ::RPI::ViewportContextPtr viewportContext = ResolveViewportContext();
                return viewportContext ? viewportContext->GetCameraTransform() : AZ::Transform::CreateIdentity();
            }

            void SetCameraTransform(const AZ::Transform& transform) override
            {
                AZ::RPI::ViewportContextPtr viewportContext = ResolveViewportContext();
                viewportContext && (viewportContext->SetCameraTransform(transform), true);
                AZ::TransformBus::Event(m_cameraEntityId, &AZ::TransformInterface::SetWorldTM, transform);
            }

            void ConnectViewMatrixChangedHandler(AZ::RPI::MatrixChangedEvent::Handler& handler) override
            {
                AZ::RPI::ViewportContextPtr viewportContext = ResolveViewportContext();
                viewportContext && (viewportContext->ConnectViewMatrixChangedHandler(handler), true);
            }

        private:
            AZ::RPI::ViewportContextPtr ResolveViewportContext() const
            {
                auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
                return viewportContextManager ? viewportContextManager->GetViewportContextById(m_viewportId) : nullptr;
            }

            AzFramework::ViewportId m_viewportId;
            AZ::EntityId m_cameraEntityId;
        };

        AzFramework::ViewportControllerPtr BuildViewportCameraController(AzFramework::ViewportId viewportId, AZ::EntityId cameraEntityId)
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

            orbitCamera->m_orbitCameras.AddCamera(orbitRotateCamera);
            orbitCamera->m_orbitCameras.AddCamera(orbitTranslateCamera);
            orbitCamera->m_orbitCameras.AddCamera(orbitScrollDollyCamera);

            auto controller = AZStd::make_shared<AtomToolsFramework::ModularViewportCameraController>();
            controller->SetCameraViewportContextBuilderCallback(
                [viewportId, cameraEntityId](AZStd::unique_ptr<AtomToolsFramework::ModularCameraViewportContext>& cameraViewportContext)
                {
                    cameraViewportContext = AZStd::make_unique<HammerCameraViewportContext>(viewportId, cameraEntityId);
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

            return controller;
        }
    } // namespace

    HammerWidget::HammerWidget(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumSize(32, 32);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_viewportWidget = new HammerRenderViewportWidget(this, false);
        m_viewportWidget->setMinimumSize(32, 32);
        m_viewportWidget->installEventFilter(this);
        layout->addWidget(m_viewportWidget);

        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();
    }

    HammerWidget::HammerWidget(QWidget* parent, QWidget& realViewport, AdoptTag)
        : QWidget(parent)
    {
        setMinimumSize(32, 32);
        m_adoptedRealViewport = &realViewport;
        realViewport.hide();
        realViewport.setParent(nullptr);
        realViewport.setParent(this);
        realViewport.setMinimumSize(32, 32);

        m_viewportWidget = FindRealRenderViewport(realViewport);
        AZ_Error("HammerWidget", m_viewportWidget, "Could not resolve the adopted real viewport's inner RenderViewportWidget");
        m_viewportWidget && (m_viewportWidget->installEventFilter(this), true);

        SyncAdoptedGeometry();

        m_sceneInitialized = true;

        ApplyActiveState();
        ApplyRenderTickState();
    }

    HammerWidget::~HammerWidget()
    {
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusDisconnect();

        auto* editorEntityAPI = AZ::Interface<AzToolsFramework::EditorEntityAPI>::Get();
        (m_cameraEntityId.IsValid() && editorEntityAPI) && (editorEntityAPI->DeleteEntityById(m_cameraEntityId), true);

        m_adoptedRealViewport && (m_adoptedRealViewport->setParent(nullptr), true);

        const bool neverInitialized = !m_sceneInitialized && m_viewportWidget;
        AZ_Warning(
            "HammerWidget", !neverInitialized || AZ::RHI::RHISystemInterface::Get(),
            "A Hammer viewport was destroyed before ever being shown, and the RHI system has already shut down; skipping "
            "deferred initialization (a known AtomToolsFramework::RenderViewportWidget shutdown issue may occur)");

        (neverInitialized && AZ::RHI::RHISystemInterface::Get()) && (m_viewportWidget->InitializeViewportContext(), true);
    }

    HammerWidget* HammerWidget::CreateAdopting(QWidget* parent, QWidget& realViewport)
    {
        return new HammerWidget(parent, realViewport, AdoptTag{});
    }

    void HammerWidget::SyncAdoptedGeometry()
    {
        m_adoptedRealViewport && (m_adoptedRealViewport->setGeometry(rect()), true);
        m_viewportWidget && (m_viewportWidget->setGeometry(QRect(QPoint(0, 0), rect().size())), true);
    }

    void HammerWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        m_adoptedRealViewport && (SyncAdoptedGeometry(), true);
        InitializeSceneIfReady();
    }

    void HammerWidget::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        m_adoptedRealViewport && (SyncAdoptedGeometry(), true);
        InitializeSceneIfReady();
    }

    bool HammerWidget::eventFilter(QObject* watched, QEvent* event)
    {
        (watched == m_viewportWidget && event->type() == QEvent::FocusIn) && (emit ViewportFocusRequested(), true);
        return QWidget::eventFilter(watched, event);
    }

    void HammerWidget::SetActive(bool active)
    {
        m_active = active;
        (m_sceneInitialized && m_viewportWidget) && (ApplyActiveState(), true);
    }

    void HammerWidget::SetRenderTickEnabled(bool enabled)
    {
        m_renderTickEnabled = enabled;
        (m_sceneInitialized && m_viewportWidget) && (ApplyRenderTickState(), true);
    }

    void HammerWidget::SetViewModes(const HammerViewModes& viewModes)
    {
        m_viewModes = viewModes;
        (m_sceneInitialized && m_viewportWidget) && (ApplyViewModes(), true);
    }

    HammerViewModes HammerWidget::GetViewModes() const
    {
        return m_viewModes;
    }

    void HammerWidget::ApplyViewModes()
    {
        AZ_Assert(m_viewportWidget, "ApplyViewModes called without an initialized viewport widget");
        AZ_Assert(m_sceneInitialized, "ApplyViewModes called before the scene was initialized");
        SetViewModePassesEnabled(m_viewportWidget->GetViewportContext(), m_viewModes);
    }

    void HammerWidget::ApplyActiveState()
    {
        AZ_Assert(m_viewportWidget, "ApplyActiveState called without an initialized viewport widget");
        AZ_Assert(m_sceneInitialized, "ApplyActiveState called before the scene was initialized");

        m_viewportWidget->SetInputProcessingEnabled(m_active);

        m_active &&
            (HammerEditorActiveViewportRequestBus::Broadcast(
                 &HammerEditorActiveViewportRequests::SetActiveViewportId, m_viewportWidget->GetId()),
             true);

        AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext();
        (viewportContext && !m_pipelineChangedHandler.IsConnected()) &&
            (m_pipelineChangedHandler = AZ::RPI::ViewportContext::PipelineChangedEvent::Handler(
                 [this, viewportContext](AZ::RPI::RenderPipelinePtr)
                 {
                     SetOverlayPassEnabled(viewportContext, m_active);
                     SetViewModePassesEnabled(viewportContext, m_viewModes);
                 }),
             viewportContext->ConnectCurrentPipelineChangedHandler(m_pipelineChangedHandler),
             true);
        SetOverlayPassEnabled(viewportContext, m_active);
        SyncViewportContextName(viewportContext, m_active, m_adoptedRealViewport != nullptr);
    }

    void HammerWidget::InitializeSceneIfReady()
    {
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

        AzFramework::ISceneSystem* sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Error("HammerWidget", sceneSystem, "AzFramework::SceneSystemInterface is not available; Hammer cannot bind a scene");

        AZStd::shared_ptr<AzFramework::Scene> mainScene;
        sceneSystem && (mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName), true);
        AZ_Error("HammerWidget", mainScene, "Main scene '%s' not found", AzFramework::Scene::MainSceneName.data());
        mainScene && (m_viewportWidget->SetScene(mainScene, true), true);

        SetupCamera();
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<SandboxEditor::ViewportManipulatorController>());
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportDisplayController>());

        ApplyActiveState();
        ApplyRenderTickState();
        ApplyViewModes();
    }

    void HammerWidget::SetupCamera()
    {
        AZ_Assert(m_viewportWidget, "SetupCamera called without an allocated viewport widget");

        m_cameraController &&
            (m_viewportWidget->GetControllerList()->Remove(m_cameraController), m_cameraController = nullptr, true);

        auto* prefabInterface = AZ::Interface<AzToolsFramework::PrefabEditorEntityOwnershipInterface>::Get();
        const bool canCreateCameraEntity =
            !m_cameraEntityId.IsValid() && prefabInterface && prefabInterface->IsRootPrefabAssigned();

        canCreateCameraEntity &&
            (AzToolsFramework::EditorEntityContextRequestBus::BroadcastResult(
                 m_cameraEntityId, &AzToolsFramework::EditorEntityContextRequests::CreateNewEditorEntity,
                 AZStd::string::format("Hammer Viewport %d Camera", m_viewportWidget->GetId()).c_str()),
             AzToolsFramework::EntityCompositionRequestBus::Broadcast(
                 &AzToolsFramework::EntityCompositionRequests::AddComponentsToEntities,
                 AzToolsFramework::EntityIdList{ m_cameraEntityId }, AZ::ComponentTypeList{ EditorCameraComponentTypeId }),
             true);

        m_cameraController = BuildViewportCameraController(m_viewportWidget->GetId(), m_cameraEntityId);
        m_viewportWidget->GetControllerList()->Add(m_cameraController);
    }

    void HammerWidget::OnContextReset()
    {
        m_cameraEntityId.SetInvalid();
        (m_sceneInitialized && m_viewportWidget) && (SetupCamera(), true);
    }

    void HammerWidget::ApplyRenderTickState()
    {
        AZ_Assert(m_viewportWidget, "ApplyRenderTickState called without an initialized viewport widget");
        SetPipelineRenderTickEnabled(m_viewportWidget->GetViewportContext(), m_renderTickEnabled);
    }

    AZ::EntityId HammerWidget::GetCameraEntityId() const
    {
        return m_cameraEntityId;
    }
}
