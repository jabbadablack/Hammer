
#include <Hammer/HammerTypeIds.h>
#include <HammerModuleInterface.h>
#include "HammerSystemComponent.h"

namespace Hammer
{
    class HammerModule
        : public HammerModuleInterface
    {
    public:
        AZ_RTTI(HammerModule, HammerModuleTypeId, HammerModuleInterface);
        AZ_CLASS_ALLOCATOR(HammerModule, AZ::SystemAllocator);
    };
}// namespace Hammer

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), Hammer::HammerModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Hammer, Hammer::HammerModule)
#endif
