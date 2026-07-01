
#pragma once

#include <Clients/HammerSystemComponent.h>

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <QPointer>

namespace Hammer
{
    class HammerWidget;

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
        void NotifyEditorInitialized() override;

        // Replaces the old perspective pane with two HammerWidget instances (one lit, one
        // wireframe) in a splitter occupying the center area.
        void EmbedViewportsInCenter();

        QPointer<HammerWidget> m_perspectiveWidget;
        QPointer<HammerWidget> m_hammerWidget;
    };
} // namespace Hammer
