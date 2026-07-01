
#include "HammerModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <Hammer/HammerTypeIds.h>

#include <Clients/HammerSystemComponent.h>

namespace Hammer
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(HammerModuleInterface,
        "HammerModuleInterface", HammerModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(HammerModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(HammerModuleInterface, AZ::SystemAllocator);

    HammerModuleInterface::HammerModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            HammerSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList HammerModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<HammerSystemComponent>(),
        };
    }
} // namespace Hammer
