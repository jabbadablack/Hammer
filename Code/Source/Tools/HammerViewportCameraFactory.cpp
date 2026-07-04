#include "HammerViewportCameraFactory.h"

#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <AzFramework/Viewport/CameraInput.h>
#include <AzToolsFramework/API/SettingsRegistryUtils.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace Hammer
{
    namespace
    {
        AzFramework::InputChannelId RegistryChannelId(const char* setting, const char* defaultId)
        {
            return AzFramework::InputChannelId(AzToolsFramework::GetRegistry(setting, AZStd::string(defaultId)).c_str());
        }

        class ViewportCameraControllerBuilder
        {
        public:
            ViewportCameraControllerBuilder()
                : m_controller(AZStd::make_shared<AtomToolsFramework::ModularViewportCameraController>())
            {
                AZ_Assert(m_controller, "Failed to allocate ModularViewportCameraController");
            }

            ViewportCameraControllerBuilder& WithViewportContext(AzFramework::ViewportId viewportId)
            {
                m_controller->SetCameraViewportContextBuilderCallback(
                    [viewportId](AZStd::unique_ptr<AtomToolsFramework::ModularCameraViewportContext>& cameraViewportContext)
                    {
                        cameraViewportContext = AZStd::make_unique<AtomToolsFramework::ModularCameraViewportContextImpl>(viewportId);
                    });
                return *this;
            }

            ViewportCameraControllerBuilder& WithDefaultPriority()
            {
                m_controller->SetCameraPriorityBuilderCallback(
                    [](AtomToolsFramework::CameraControllerPriorityFn& priorityFn)
                    {
                        priorityFn = AtomToolsFramework::DefaultCameraControllerPriority;
                    });
                return *this;
            }

            ViewportCameraControllerBuilder& WithCameraProps()
            {
                m_controller->SetCameraPropsBuilderCallback(
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
                return *this;
            }

            ViewportCameraControllerBuilder& WithCameras(
                AZStd::shared_ptr<AzFramework::RotateCameraInput> rotateCamera,
                AZStd::shared_ptr<AzFramework::TranslateCameraInput> translateCamera,
                AZStd::shared_ptr<AzFramework::LookScrollTranslationCameraInput> scrollCamera,
                AZStd::shared_ptr<AzFramework::OrbitCameraInput> orbitCamera)
            {
                m_controller->SetCameraListBuilderCallback(
                    [rotateCamera, translateCamera, scrollCamera, orbitCamera](AzFramework::Cameras& cameras)
                    {
                        cameras.AddCamera(rotateCamera);
                        cameras.AddCamera(translateCamera);
                        cameras.AddCamera(scrollCamera);
                        cameras.AddCamera(orbitCamera);
                    });
                return *this;
            }

            AZStd::shared_ptr<AtomToolsFramework::ModularViewportCameraController> Build() const
            {
                return m_controller;
            }

        private:
            AZStd::shared_ptr<AtomToolsFramework::ModularViewportCameraController> m_controller;
        };
    } // namespace

    AzFramework::ViewportControllerPtr CreateViewportCameraController(AzFramework::ViewportId viewportId)
    {
        AZ_Assert(viewportId != AzFramework::InvalidViewportId, "CreateViewportCameraController called with an invalid ViewportId");

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
                    direction * aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultOrbitDistance", 20.0));
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

        AzFramework::ViewportControllerPtr controller = ViewportCameraControllerBuilder()
            .WithViewportContext(viewportId)
            .WithDefaultPriority()
            .WithCameraProps()
            .WithCameras(rotateCamera, translateCamera, scrollCamera, orbitCamera)
            .Build();
        AZ_Assert(controller, "CreateViewportCameraController failed to build a camera controller");
        return controller;
    }
} // namespace Hammer
