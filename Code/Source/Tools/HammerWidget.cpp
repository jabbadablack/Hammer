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
        auto* viewport1 = new AtomToolsFramework::RenderViewportWidget(this);
        viewport1->InitializeViewportContext();

        if (auto* sceneSystem = AzFramework::SceneSystemInterface::Get())
        {
            if (AZStd::shared_ptr<AzFramework::Scene> mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName))
            {
                // Bind the scene without the default lit pipeline; Hammer builds its own
                // wireframe-only pipeline below instead.
                viewport1->SetScene(mainScene, /*useDefaultRenderPipeline*/ false);

                if (auto viewportContext = viewport1->GetViewportContext())
                {
                    if (auto windowContext = viewportContext->GetWindowContext())
                    {
                        AZ::RPI::RenderPipelineDescriptor pipelineDesc;
                        pipelineDesc.m_name = "HammerWireframePipeline";
                        pipelineDesc.m_rootPassTemplate = "HammerWireframePipelineTemplate";

                        if (auto pipeline = AZ::RPI::RenderPipeline::CreateRenderPipelineForWindow(pipelineDesc, *windowContext))
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

        auto rotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(AzFramework::InputDeviceMouse::Button::Right);
        auto translateCamera = AZStd::make_shared<AzFramework::TranslateCameraInput>(
            translateIds, AzFramework::LookTranslation, AzFramework::TranslatePivotLook);
        auto scrollCamera = AZStd::make_shared<AzFramework::LookScrollTranslationCameraInput>();

        auto cameraController = AZStd::make_shared<AtomToolsFramework::ModularViewportCameraController>();
        cameraController->SetCameraViewportContextBuilderCallback(
            [viewportId = viewport1->GetId()](AZStd::unique_ptr<AtomToolsFramework::ModularCameraViewportContext>& cameraViewportContext)
            {
                AZ_Warning("HammerWidget", false, "Camera controller instance created for viewport %d", static_cast<int>(viewportId));
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
