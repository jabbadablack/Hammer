
#pragma once

#include <Clients/HammerSystemComponent.h>

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

namespace Hammer
{
    /// System component for Hammer editor
    class HammerEditorSystemComponent
        : public HammerSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = HammerSystemComponent;
    public:
        AZ_COMPONENT_DECL(HammerEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        HammerEditorSystemComponent();
        ~HammerEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::EditorEventsBus overrides ...
        void NotifyRegisterViews() override;
    };
} // namespace Hammer
