
#include <Hammer/HammerTypeIds.h>
#include <HammerModuleInterface.h>
#include "HammerEditorSystemComponent.h"

void InitHammerResources()
{
    // register our .qrc file since this is being loaded from a separate gem
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

            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                HammerEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
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
