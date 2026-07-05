
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
