#include "HammerWidget.h"
#include <QVBoxLayout>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/WindowContext.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/CameraInput.h>
#include <AzFramework/Viewport/ViewportControllerList.h>

namespace Hammer
{
    HammerWidget::HammerWidget(QWidget* parent)
        : QWidget(parent)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        AtomToolsFramework::RenderViewportWidget* viewport1 = new AtomToolsFramework::RenderViewportWidget(this);
        viewport1->InitializeViewportContext();

        AzFramework::ISceneSystem* sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Error("HammerWidget", sceneSystem, "AzFramework::SceneSystemInterface is not available; Hammer cannot bind a scene");
        if (sceneSystem)
        {
            AZStd::shared_ptr<AzFramework::Scene> mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName);
            AZ_Error("HammerWidget", mainScene, "Main scene '%s' not found", AzFramework::Scene::MainSceneName.data());
            if (mainScene)
            {
                // Bind the scene without the default lit pipeline; Hammer builds its own
                // wireframe-only pipeline below instead.
                viewport1->SetScene(mainScene, /*useDefaultRenderPipeline*/ false);

                AZ::RPI::ViewportContextPtr viewportContext = viewport1->GetViewportContext();
                AZ_Error("HammerWidget", viewportContext, "RenderViewportWidget has no ViewportContext after SetScene");
                if (viewportContext)
                {
                    AZ::RPI::WindowContextSharedPtr windowContext = viewportContext->GetWindowContext();
                    AZ_Error("HammerWidget", windowContext, "ViewportContext has no WindowContext");
                    if (windowContext)
                    {
                        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
                        pipelineDesc.m_name = "HammerWireframePipeline";
                        pipelineDesc.m_rootPassTemplate = "HammerWireframePipelineTemplate";

                        AZ::RPI::RenderPipelinePtr pipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *windowContext);
                        AZ_Error(
                            "HammerWidget", pipeline,
                            "Failed to create HammerWireframePipeline; is HammerWireframePipelineTemplate registered/compiled?");
                        if (pipeline)
                        {
                            // Adding the pipeline to the scene (bound to our window) is enough:
                            // ViewportContext automatically binds its own view to it once the
                            // pipeline's pass tree (and its view tags) are ready. Calling
                            // SetDefaultView() here ourselves is premature and asserts.
                            viewportContext->GetRenderScene()->AddRenderPipeline(pipeline);
                            m_pipeline = pipeline;
                        }
                    }
                }
            }
        }

        AzFramework::TranslateCameraInputChannelIds translateIds;
        translateIds.m_forwardChannelId = AzFramework::InputDeviceKeyboard::Key::AlphanumericW;
        translateIds.m_backwardChannelId = AzFramework::InputDeviceKeyboard::Key::AlphanumericS;
        translateIds.m_leftChannelId = AzFramework::InputDeviceKeyboard::Key::AlphanumericA;
        translateIds.m_rightChannelId = AzFramework::InputDeviceKeyboard::Key::AlphanumericD;
        translateIds.m_upChannelId = AzFramework::InputDeviceKeyboard::Key::AlphanumericE;
        translateIds.m_downChannelId = AzFramework::InputDeviceKeyboard::Key::AlphanumericQ;
        translateIds.m_boostChannelId = AzFramework::InputDeviceKeyboard::Key::ModifierShiftL;

        AZStd::shared_ptr<AzFramework::RotateCameraInput> rotateCamera =
            AZStd::make_shared<AzFramework::RotateCameraInput>(AzFramework::InputDeviceMouse::Button::Right);
        AZStd::shared_ptr<AzFramework::TranslateCameraInput> translateCamera = AZStd::make_shared<AzFramework::TranslateCameraInput>(
            translateIds, AzFramework::LookTranslation, AzFramework::TranslatePivotLook);
        AZStd::shared_ptr<AzFramework::LookScrollTranslationCameraInput> scrollCamera =
            AZStd::make_shared<AzFramework::LookScrollTranslationCameraInput>();

        AZStd::shared_ptr<AtomToolsFramework::ModularViewportCameraController> cameraController =
            AZStd::make_shared<AtomToolsFramework::ModularViewportCameraController>();
        cameraController->SetCameraViewportContextBuilderCallback(
            [viewportId = viewport1->GetId()](AZStd::unique_ptr<AtomToolsFramework::ModularCameraViewportContext>& cameraViewportContext)
            {
                cameraViewportContext = AZStd::make_unique<AtomToolsFramework::ModularCameraViewportContextImpl>(viewportId);
            });
        cameraController->SetCameraPriorityBuilderCallback(
            [](AtomToolsFramework::CameraControllerPriorityFn& priorityFn)
            {
                priorityFn = AtomToolsFramework::DefaultCameraControllerPriority;
            });
        cameraController->SetCameraPropsBuilderCallback(
            [](AzFramework::CameraProps& cameraProps)
            {
                cameraProps.m_rotateSmoothnessFn = [] { return 5.0f; };
                cameraProps.m_translateSmoothnessFn = [] { return 5.0f; };
                cameraProps.m_rotateSmoothingEnabledFn = [] { return true; };
                cameraProps.m_translateSmoothingEnabledFn = [] { return true; };
            });
        cameraController->SetCameraListBuilderCallback(
            [rotateCamera, translateCamera, scrollCamera](AzFramework::Cameras& cameras)
            {
                cameras.AddCamera(rotateCamera);
                cameras.AddCamera(translateCamera);
                cameras.AddCamera(scrollCamera);
            });

        viewport1->GetControllerList()->Add(cameraController);

        mainLayout->addWidget(viewport1);
        setLayout(mainLayout);
    }

    HammerWidget::~HammerWidget()
    {
        // Must remove the pipeline from the scene here, while the viewport (and the window
        // context/swapchain it owns) is still alive. Qt destroys child widgets after this
        // destructor body runs.
        if (m_pipeline)
        {
            m_pipeline->RemoveFromScene();
            m_pipeline = nullptr;
        }
    }
}
