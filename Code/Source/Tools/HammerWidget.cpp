#include "HammerWidget.h"
#include "HammerQtEnvironment.h"
#include "HammerRenderViewportWidget.h"
#include "HammerViewportCameraComponent.h"
#include "HammerViewportManipulatorController.h"

#include <AzCore/Interface/Interface.h>
#include <Hammer/HammerEditorViewportBus.h>

#include <QEvent>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/CameraInput.h>
#include <AzFramework/Viewport/ViewportControllerList.h>
#include <AzToolsFramework/API/EditorEntityAPI.h>
#include <AzToolsFramework/API/SettingsRegistryUtils.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Entity/PrefabEditorEntityOwnershipInterface.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

#include <Tools/ui_HammerWidget.h>

namespace Hammer::Platform
{
    void SyncAdoptedViewportGeometry(QWidget& adoptedRealViewport, const QRect& targetGeometry);
} // namespace Hammer::Platform

namespace Hammer
{
    namespace
    {
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

        void SyncViewportContextName(
            AZ::RPI::ViewportContextPtr viewportContext, AzFramework::ViewportId viewportId, bool active, bool isAdoptedViewport)
        {
            AZ_Assert(viewportContext, "SyncViewportContextName called with a null ViewportContext");
            AZ_Assert(viewportId != AzFramework::InvalidViewportId, "SyncViewportContextName called with an invalid ViewportId");

            auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
            AZ_Error("HammerWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");

            const AZ::Name defaultName = viewportContextManager->GetDefaultViewportContextName();
            const AZ::Name currentName = viewportContext->GetName();

            (isAdoptedViewport && active && currentName != defaultName) &&
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

        class HammerCameraViewportContext
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

        AzFramework::InputChannelId RegistryChannelId(const char* setting, const char* defaultId)
        {
            return AzFramework::InputChannelId(AzToolsFramework::GetRegistry(setting, AZStd::string(defaultId)).c_str());
        }

        AzFramework::ViewportControllerPtr BuildViewportCameraController(AzFramework::ViewportId viewportId, AZ::EntityId cameraEntityId)
        {
            AZ_Assert(viewportId != AzFramework::InvalidViewportId, "BuildViewportCameraController called with an invalid ViewportId");

            AzFramework::TranslateCameraInputChannelIds translateIds;
            translateIds.m_forwardChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateForwardId", "keyboard_key_alphanumeric_W");
            translateIds.m_backwardChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateBackwardId", "keyboard_key_alphanumeric_S");
            translateIds.m_leftChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateLeftId", "keyboard_key_alphanumeric_A");
            translateIds.m_rightChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateRightId", "keyboard_key_alphanumeric_D");
            translateIds.m_upChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateUpId", "keyboard_key_alphanumeric_E");
            translateIds.m_downChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateUpDownId", "keyboard_key_alphanumeric_Q");
            translateIds.m_boostChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/TranslateBoostId", "keyboard_key_modifier_shift_l");

            const AzFramework::InputChannelId freeLookChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/FreeLookId", "mouse_button_right");
            auto rotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(freeLookChannelId);
            AZ_Assert(rotateCamera, "Failed to allocate RotateCameraInput");
            rotateCamera->m_rotateSpeedFn = []
            { return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSpeed", 0.005)); };

            const auto captureCursorForLook = []
            { return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/CaptureCursorLook", true); };
            const auto hideCursor = [viewportId, captureCursorForLook]
            {
                captureCursorForLook() &&
                    (AzToolsFramework::ViewportInteraction::ViewportMouseCursorRequestBus::Event(
                         viewportId, &AzToolsFramework::ViewportInteraction::ViewportMouseCursorRequestBus::Events::BeginCursorCapture),
                     true);
            };
            const auto showCursor = [viewportId, captureCursorForLook]
            {
                captureCursorForLook() &&
                    (AzToolsFramework::ViewportInteraction::ViewportMouseCursorRequestBus::Event(
                         viewportId, &AzToolsFramework::ViewportInteraction::ViewportMouseCursorRequestBus::Events::EndCursorCapture),
                     true);
            };
            rotateCamera->SetActivationBeganFn(hideCursor);
            rotateCamera->SetActivationEndedFn(showCursor);

            auto translateCamera = AZStd::make_shared<AzFramework::TranslateCameraInput>(
                translateIds, AzFramework::LookTranslation, AzFramework::TranslatePivotLook);
            AZ_Assert(translateCamera, "Failed to allocate TranslateCameraInput");
            translateCamera->m_translateSpeedFn = []
            { return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSpeed", 10.0)); };
            translateCamera->m_boostMultiplierFn = []
            { return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/BoostMultiplier", 3.0)); };

            auto scrollCamera = AZStd::make_shared<AzFramework::LookScrollTranslationCameraInput>();
            scrollCamera->m_scrollSpeedFn = []
            { return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DollyScrollSpeed", 0.02)); };

            AZ_Assert(scrollCamera, "Failed to allocate LookScrollTranslationCameraInput");

            const AzFramework::InputChannelId orbitChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/OrbitId", "keyboard_key_modifier_alt_l");
            auto orbitCamera = AZStd::make_shared<AzFramework::OrbitCameraInput>(orbitChannelId);
            AZ_Assert(orbitCamera, "Failed to allocate OrbitCameraInput");
            orbitCamera->SetPivotFn(
                [](const AZ::Vector3& position, const AZ::Vector3& direction)
                {
                    return position +
                        direction *
                        aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultOrbitDistance", 20.0));
                });

            const AzFramework::InputChannelId orbitLookChannelId =
                RegistryChannelId("/Amazon/Preferences/Editor/Camera/OrbitLookId", "mouse_button_left");
            auto orbitRotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(orbitLookChannelId);
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
            AZ_Assert(controller, "BuildViewportCameraController failed to allocate a ModularViewportCameraController");

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
                    cameraProps.m_rotateSmoothnessFn = []
                    { return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSmoothness", 5.0)); };
                    cameraProps.m_translateSmoothnessFn = []
                    { return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSmoothness", 5.0)); };
                    cameraProps.m_rotateSmoothingEnabledFn = []
                    { return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSmoothing", true); };
                    cameraProps.m_translateSmoothingEnabledFn = []
                    { return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSmoothing", true); };
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
        , m_ui(new Ui::HammerWidgetClass())
    {
        m_ui->setupUi(this);

        m_viewportWidget = new HammerRenderViewportWidget(this, false);
        AZ_Assert(m_viewportWidget, "Failed to allocate the Hammer render viewport widget");
        m_viewportWidget->installEventFilter(this);

        m_ui->mainLayout->addWidget(m_viewportWidget);

        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();
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

    HammerWidget::HammerWidget(QWidget* parent, QWidget& realViewport, AdoptTag)
        : QWidget(parent)
    {
        m_adoptedRealViewport = &realViewport;
        realViewport.hide();
        realViewport.setParent(nullptr);
        realViewport.setParent(this);

        m_viewportWidget = FindRealRenderViewport(&realViewport);
        AZ_Error("HammerWidget", m_viewportWidget, "Could not resolve the adopted real viewport's inner RenderViewportWidget");
        m_viewportWidget && (m_viewportWidget->installEventFilter(this), true);

        SyncAdoptedGeometry();

        m_sceneInitialized = true;

        ApplyActiveState();
        ApplyRenderTickState();
    }

    // Forces both the adopted widget and its inner render surface to match our own size,
    // rather than trusting the adopted widget's own internal layout to cascade the resize
    // down to its render surface on its own.
    void HammerWidget::SyncAdoptedGeometry()
    {
        m_adoptedRealViewport && (Platform::SyncAdoptedViewportGeometry(*m_adoptedRealViewport, rect()), true);
        m_viewportWidget &&
            (Platform::SyncAdoptedViewportGeometry(*m_viewportWidget, QRect(QPoint(0, 0), rect().size())), true);
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
                 [this, viewportContext](AZ::RPI::RenderPipelinePtr pipeline)
                 {
                     AZ_UNUSED(pipeline);
                     SetOverlayPassEnabled(viewportContext, m_active);
                 }),
             viewportContext->ConnectCurrentPipelineChangedHandler(m_pipelineChangedHandler),
             true);
        SetOverlayPassEnabled(viewportContext, m_active);
        SyncViewportContextName(viewportContext, m_viewportWidget->GetId(), m_active, m_adoptedRealViewport != nullptr);
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
        mainScene && (m_viewportWidget->SetScene(mainScene, /*useDefaultRenderPipeline*/ true), true);

        SetupCamera();
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>());

        ApplyActiveState();
        ApplyRenderTickState();
    }

    void HammerWidget::SetupCamera()
    {
        AZ_Assert(m_viewportWidget, "SetupCamera called without an allocated viewport widget");

        m_cameraController &&
            (m_viewportWidget->GetControllerList()->Remove(m_cameraController), m_cameraController = nullptr, true);

        auto* prefabEditorEntityOwnershipInterface = AZ::Interface<AzToolsFramework::PrefabEditorEntityOwnershipInterface>::Get();
        const bool canCreateCameraEntity =
            !m_cameraEntityId.IsValid() && prefabEditorEntityOwnershipInterface && prefabEditorEntityOwnershipInterface->IsRootPrefabAssigned();

        canCreateCameraEntity &&
            (AzToolsFramework::EditorEntityContextRequestBus::BroadcastResult(
                 m_cameraEntityId, &AzToolsFramework::EditorEntityContextRequests::CreateNewEditorEntity,
                 AZStd::string::format("Hammer Viewport %d Camera", m_viewportWidget->GetId()).c_str()),
             AzToolsFramework::AddComponents<AzToolsFramework::Components::TransformComponent, HammerViewportCameraComponent>::ToEntities(
                 m_cameraEntityId),
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
