#include "HammerViewportCameraComponent.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/CameraBus.h>

namespace Hammer
{
    void HammerViewportCameraComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        serializeContext &&
            (serializeContext->Class<HammerViewportCameraComponent, AzToolsFramework::Components::EditorComponentBase>()->Version(0),
             true);
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
        AzToolsFramework::Components::EditorComponentBase::Activate();
        Camera::CameraNotificationBus::Broadcast(&Camera::CameraNotificationBus::Events::OnCameraAdded, GetEntityId());
    }

    void HammerViewportCameraComponent::Deactivate()
    {
        Camera::CameraNotificationBus::Broadcast(&Camera::CameraNotificationBus::Events::OnCameraRemoved, GetEntityId());
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }
} // namespace Hammer
