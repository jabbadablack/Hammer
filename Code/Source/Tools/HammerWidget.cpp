#include "HammerWidget.h"
#include <QVBoxLayout>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
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
                viewport1->SetScene(mainScene);
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
                cameraViewportContext = AZStd::make_unique<AtomToolsFramework::ModularCameraViewportContextImpl>(viewportId);
            });
        cameraController->SetCameraPriorityBuilderCallback(
            [](AtomToolsFramework::CameraControllerPriorityFn& priorityFn)
            {
                priorityFn = AtomToolsFramework::DefaultCameraControllerPriority;
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
}
