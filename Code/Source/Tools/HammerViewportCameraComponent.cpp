#include "HammerViewportCameraComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/CameraBus.h>

#include <Hammer/HammerTypeIds.h>

namespace Hammer
{
    AZ_COMPONENT_IMPL(HammerViewportCameraComponent, "HammerViewportCameraComponent", HammerViewportCameraComponentTypeId);

    void HammerViewportCameraComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        serializeContext && (serializeContext->Class<HammerViewportCameraComponent, AZ::Component>()->Version(0), true);
    }

    void HammerViewportCameraComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("CameraService"));
    }

    void HammerViewportCameraComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void HammerViewportCameraComponent::Activate()
    {
        Camera::CameraNotificationBus::Broadcast(&Camera::CameraNotificationBus::Events::OnCameraAdded, GetEntityId());
    }

    void HammerViewportCameraComponent::Deactivate()
    {
        Camera::CameraNotificationBus::Broadcast(&Camera::CameraNotificationBus::Events::OnCameraRemoved, GetEntityId());
    }
} // namespace Hammer
