
#include <Hammer/HammerTypeIds.h>
#include <HammerModuleInterface.h>
#include "HammerEditorSystemComponent.h"

static void InitResources()
{
    Q_INIT_RESOURCE(Hammer);
}

namespace Hammer
{
    class EditorModule
        : public ModuleInterface
    {
    public:
        AZ_RTTI(EditorModule, EditorModuleTypeId, ModuleInterface);
        AZ_CLASS_ALLOCATOR(EditorModule, AZ::SystemAllocator);

        EditorModule()
        {
            InitResources();

            m_descriptors.insert(m_descriptors.end(), {
                EditorSystemComponent::CreateDescriptor(),
            });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<EditorSystemComponent>(),
            };
        }
    };
}// namespace Hammer

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), Hammer::EditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Hammer_Editor, Hammer::EditorModule)
#endif
