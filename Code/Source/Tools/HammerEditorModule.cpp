
#include <Hammer/HammerTypeIds.h>
#include <HammerModuleInterface.h>
#include "HammerEditorSystemComponent.h"
#include "HammerViewportCameraComponent.h"

void InitHammerResources()
{
    Q_INIT_RESOURCE(Hammer);
}

namespace Hammer
{
    class HammerEditorModule
        : public HammerModuleInterface
    {
    public:
        AZ_RTTI(HammerEditorModule, HammerEditorModuleTypeId, HammerModuleInterface);
        AZ_CLASS_ALLOCATOR(HammerEditorModule, AZ::SystemAllocator);

        HammerEditorModule()
        {
            InitHammerResources();

            m_descriptors.insert(m_descriptors.end(), {
                HammerEditorSystemComponent::CreateDescriptor(),
                HammerViewportCameraComponent::CreateDescriptor(),
            });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<HammerEditorSystemComponent>(),
            };
        }
    };
}// namespace Hammer

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), Hammer::HammerEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Hammer_Editor, Hammer::HammerEditorModule)
#endif
