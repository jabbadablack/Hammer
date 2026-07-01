
#pragma once

#include <Clients/HammerSystemComponent.h>

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <QPointer>

class QDockWidget;

namespace Hammer
{
    class HammerViewportLayoutWidget;

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

        // Registers Hammer's viewport (a HammerViewportLayoutWidget hosting 1-4 viewports) as a
        // normal AzToolsFramework view pane, so Qt's own QtViewPaneManager owns its
        // creation/teardown, and it shows up as a toggleable entry in the Tools menu. Must run
        // from NotifyRegisterViews() (not Activate()) - that's the point in Editor startup when
        // the EditorRequests bus handler that actually implements pane registration is guaranteed
        // to be connected; registering earlier silently no-ops and later InstanceViewPane calls
        // fail to find the pane.
        void RegisterViewportPane();

        // Instances the registered pane and swaps it in as the center content, replacing the
        // real perspective viewport (which isn't itself a registered pane, so it can only be
        // located by walking the widget hierarchy).
        void EmbedViewportInCenter();

        QPointer<HammerViewportLayoutWidget> m_viewportLayoutWidget;
        QPointer<QDockWidget> m_paneDockWidget;
    };
} // namespace Hammer
