
#include "HammerModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <Hammer/HammerTypeIds.h>

#include <Clients/HammerSystemComponent.h>

namespace Hammer
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(ModuleInterface,
        "HammerModuleInterface", ModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(ModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(ModuleInterface, AZ::SystemAllocator);

    ModuleInterface::ModuleInterface()
    {
        m_descriptors.insert(m_descriptors.end(), {
            SystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList ModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<SystemComponent>(),
        };
    }
} // namespace Hammer
