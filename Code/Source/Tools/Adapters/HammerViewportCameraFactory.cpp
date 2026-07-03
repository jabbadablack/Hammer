#include "HammerViewportCameraFactory.h"
#include <Hammer/IHammerSettingsProvider.h>

#include <AtomToolsFramework/Viewport/ModularViewportCameraController.h>
#include <AzCore/Interface/Interface.h>
#include <AzFramework/Viewport/CameraInput.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace Hammer
{
    namespace
    {
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

            ViewportCameraControllerBuilder& WithCameraProps(IHammerSettingsProvider* settings)
            {
                m_controller->SetCameraPropsBuilderCallback(
                    [settings](AzFramework::CameraProps& cameraProps)
                    {
                        cameraProps.m_rotateSmoothnessFn = [settings] { return settings->CameraRotateSmoothness(); };
                        cameraProps.m_translateSmoothnessFn = [settings] { return settings->CameraTranslateSmoothness(); };
                        cameraProps.m_rotateSmoothingEnabledFn = [settings] { return settings->CameraRotateSmoothingEnabled(); };
                        cameraProps.m_translateSmoothingEnabledFn = [settings] { return settings->CameraTranslateSmoothingEnabled(); };
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
        auto* settings = AZ::Interface<IHammerSettingsProvider>::Get();
        AZ_Assert(settings, "CreateViewportCameraController requires IHammerSettingsProvider to be registered");

        AzFramework::TranslateCameraInputChannelIds translateIds;
        translateIds.m_forwardChannelId = settings->CameraTranslateForwardChannelId();
        translateIds.m_backwardChannelId = settings->CameraTranslateBackwardChannelId();
        translateIds.m_leftChannelId = settings->CameraTranslateLeftChannelId();
        translateIds.m_rightChannelId = settings->CameraTranslateRightChannelId();
        translateIds.m_upChannelId = settings->CameraTranslateUpChannelId();
        translateIds.m_downChannelId = settings->CameraTranslateDownChannelId();
        translateIds.m_boostChannelId = settings->CameraTranslateBoostChannelId();

        auto rotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(settings->CameraFreeLookChannelId());
        AZ_Assert(rotateCamera, "Failed to allocate RotateCameraInput");
        rotateCamera->m_rotateSpeedFn = [settings] { return settings->CameraRotateSpeed(); };

        const auto captureCursorForLook = [settings] { return settings->CameraCaptureCursorForLook(); };
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
        translateCamera->m_translateSpeedFn = [settings] { return settings->CameraTranslateSpeed(); };
        translateCamera->m_boostMultiplierFn = [settings] { return settings->CameraBoostMultiplier(); };

        auto scrollCamera = AZStd::make_shared<AzFramework::LookScrollTranslationCameraInput>();
        scrollCamera->m_scrollSpeedFn = [settings] { return settings->CameraDollyScrollSpeed(); };

        AZ_Assert(scrollCamera, "Failed to allocate LookScrollTranslationCameraInput");

        auto orbitCamera = AZStd::make_shared<AzFramework::OrbitCameraInput>(settings->CameraOrbitChannelId());
        AZ_Assert(orbitCamera, "Failed to allocate OrbitCameraInput");
        orbitCamera->SetPivotFn(
            [settings](const AZ::Vector3& position, const AZ::Vector3& direction)
            {
                return position + direction * settings->CameraDefaultOrbitDistance();
            });

        auto orbitRotateCamera = AZStd::make_shared<AzFramework::RotateCameraInput>(settings->CameraOrbitLookChannelId());
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

        return ViewportCameraControllerBuilder()
            .WithViewportContext(viewportId)
            .WithDefaultPriority()
            .WithCameraProps(settings)
            .WithCameras(rotateCamera, translateCamera, scrollCamera, orbitCamera)
            .Build();
    }
} // namespace Hammer
