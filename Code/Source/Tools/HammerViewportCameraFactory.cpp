#include "HammerViewportCameraFactory.h"

#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <AzFramework/Viewport/CameraInput.h>
#include <AzToolsFramework/API/SettingsRegistryUtils.h>

namespace Hammer
{
    namespace
    {
        // Same Settings Registry keys the real Editor viewport's camera reads (Code/Editor/EditorViewportSettings.cpp),
        // so Hammer's viewports pick up whatever camera preferences the user has already configured.
        AzFramework::InputChannelId RegistryChannelId(const char* setting, const char* defaultId)
        {
            return AzFramework::InputChannelId(AzToolsFramework::GetRegistry(setting, AZStd::string(defaultId)).c_str());
        }
    }

    AzFramework::ViewportControllerPtr CreateViewportCameraController(AzFramework::ViewportId viewportId)
    {
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

        auto rotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(
            RegistryChannelId("/Amazon/Preferences/Editor/Camera/FreeLookId", "mouse_button_right"));
        rotateCamera->m_rotateSpeedFn = []
        {
            return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSpeed", 0.005));
        };

        auto translateCamera = AZStd::make_shared<AzFramework::TranslateCameraInput>(
            translateIds, AzFramework::LookTranslation, AzFramework::TranslatePivotLook);
        translateCamera->m_translateSpeedFn = []
        {
            return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSpeed", 10.0));
        };
        translateCamera->m_boostMultiplierFn = []
        {
            return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/BoostMultiplier", 3.0));
        };

        auto scrollCamera = AZStd::make_shared<AzFramework::LookScrollTranslationCameraInput>();
        scrollCamera->m_scrollSpeedFn = []
        {
            return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DollyScrollSpeed", 0.02));
        };

        auto orbitCamera = AZStd::make_shared<AzFramework::OrbitCameraInput>(
            RegistryChannelId("/Amazon/Preferences/Editor/Camera/OrbitId", "keyboard_key_modifier_alt_l"));
        orbitCamera->SetPivotFn(
            [](const AZ::Vector3& position, const AZ::Vector3& direction)
            {
                const float distance =
                    aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultOrbitDistance", 20.0));
                return position + direction * distance;
            });

        auto orbitRotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(
            RegistryChannelId("/Amazon/Preferences/Editor/Camera/OrbitLookId", "mouse_button_left"));
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
                cameraProps.m_rotateSmoothnessFn = []
                {
                    return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSmoothness", 5.0));
                };
                cameraProps.m_translateSmoothnessFn = []
                {
                    return aznumeric_cast<float>(
                        AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSmoothness", 5.0));
                };
                cameraProps.m_rotateSmoothingEnabledFn = []
                {
                    return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSmoothing", true);
                };
                cameraProps.m_translateSmoothingEnabledFn = []
                {
                    return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSmoothing", true);
                };
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
