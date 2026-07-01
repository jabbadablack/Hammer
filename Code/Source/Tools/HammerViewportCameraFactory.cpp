#include "HammerViewportCameraFactory.h"

#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Viewport/CameraInput.h>

namespace Hammer
{
    AzFramework::ViewportControllerPtr CreateViewportCameraController(AzFramework::ViewportId viewportId)
    {
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

        // Alt held + LMB drag orbits around a point in front of the camera, matching the default viewport.
        auto orbitCamera = AZStd::make_shared<AzFramework::OrbitCameraInput>(AzFramework::InputDeviceKeyboard::Key::ModifierAltL);
        orbitCamera->SetPivotFn(
            [](const AZ::Vector3& position, const AZ::Vector3& direction)
            {
                constexpr float pivotDistance = 10.0f;
                return position + direction * pivotDistance;
            });

        auto orbitRotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(AzFramework::InputDeviceMouse::Button::Left);
        auto orbitTranslateCamera = AZStd::make_shared<AzFramework::TranslateCameraInput>(
            translateIds, AzFramework::LookTranslation, AzFramework::TranslateOffsetOrbit);
        auto orbitScrollDollyCamera = AZStd::make_shared<AzFramework::OrbitScrollDollyCameraInput>();

        orbitCamera->m_orbitCameras.AddCamera(orbitRotateCamera);
        orbitCamera->m_orbitCameras.AddCamera(orbitTranslateCamera);
        orbitCamera->m_orbitCameras.AddCamera(orbitScrollDollyCamera);

        auto cameraController = AZStd::make_shared<AtomToolsFramework::ModularViewportCameraController>();
        cameraController->SetCameraViewportContextBuilderCallback(
            [viewportId](AZStd::unique_ptr<AtomToolsFramework::ModularCameraViewportContext>& cameraViewportContext)
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
            [rotateCamera, translateCamera, scrollCamera, orbitCamera](AzFramework::Cameras& cameras)
            {
                cameras.AddCamera(rotateCamera);
                cameras.AddCamera(translateCamera);
                cameras.AddCamera(scrollCamera);
                cameras.AddCamera(orbitCamera);
            });

        return cameraController;
    }
}
