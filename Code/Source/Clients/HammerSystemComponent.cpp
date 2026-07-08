
#include "HammerSystemComponent.h"

#include <Hammer/HammerTypeIds.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace Hammer
{
    AZ_COMPONENT_IMPL(SystemComponent, "HammerSystemComponent", SystemComponentTypeId);

    void SystemComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        serializeContext && (serializeContext->Class<SystemComponent, AZ::Component>()->Version(0), true);
    }

    void SystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("HammerService"));
    }

    void SystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("HammerService"));
    }

    void SystemComponent::Activate()
    {
    }

    void SystemComponent::Deactivate()
    {
    }
} // namespace Hammer
