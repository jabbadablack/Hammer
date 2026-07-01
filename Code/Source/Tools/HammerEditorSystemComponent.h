
#pragma once

#include <Clients/HammerSystemComponent.h>

#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <QPointer>

class QWidget;

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

        // Pops the Editor's real main viewport widget into its own top-level window, then creates
        // Hammer's own viewport window beside it.
        void SplitIntoViewportWindows();

        AZ::RPI::PassSystemInterface::OnReadyLoadTemplatesEvent::Handler m_loadTemplatesHandler;
        QPointer<QWidget> m_mainViewportWidget;
        QPointer<HammerWidget> m_hammerWidget;
    };
} // namespace Hammer
