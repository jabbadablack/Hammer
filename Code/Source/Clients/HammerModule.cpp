
#include <Hammer/HammerTypeIds.h>
#include <HammerModuleInterface.h>
#include "HammerSystemComponent.h"

namespace Hammer
{
    class Module
        : public ModuleInterface
    {
    public:
        AZ_RTTI(Module, ModuleTypeId, ModuleInterface);
        AZ_CLASS_ALLOCATOR(Module, AZ::SystemAllocator);
    };
}// namespace Hammer

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), Hammer::Module)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Hammer, Hammer::Module)
#endif
