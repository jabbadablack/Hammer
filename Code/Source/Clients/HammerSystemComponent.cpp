
#include "HammerSystemComponent.h"

#include <Hammer/HammerTypeIds.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace Hammer
{
    AZ_COMPONENT_IMPL(HammerSystemComponent, "HammerSystemComponent",
        HammerSystemComponentTypeId);

    void HammerSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        serializeContext && (serializeContext->Class<HammerSystemComponent, AZ::Component>()->Version(0), true);
    }

    void HammerSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("HammerService"));
    }

    void HammerSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("HammerService"));
    }

    void HammerSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void HammerSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    HammerSystemComponent::HammerSystemComponent() = default;

    HammerSystemComponent::~HammerSystemComponent() = default;

    void HammerSystemComponent::Init()
    {
    }

    void HammerSystemComponent::Activate()
    {
        AZ::TickBus::Handler::BusConnect();
    }

    void HammerSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
    }

    void HammerSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
    }

} // namespace Hammer
