
#pragma once

#include <Clients/HammerSystemComponent.h>

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <AzCore/std/containers/vector.h>

#include <QPointer>

class QDockWidget;
class QMainWindow;
class QToolButton;

namespace Hammer
{
    class HammerViewportLayoutWidget;

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

        void Activate() override;
        void Deactivate() override;

        void NotifyRegisterViews() override;
        void NotifyEditorInitialized() override;

        void RegisterViewportPane();
        void EmbedViewportInCenter();
        void CreateViewportCountButtons();
        void DestroyViewportCountButtons();

        QPointer<HammerViewportLayoutWidget> m_viewportLayoutWidget;
        QPointer<QDockWidget> m_paneDockWidget;
        AZStd::vector<QPointer<QToolButton>> m_viewportCountButtons;
        class ViewportSizeFilter* m_viewportFilter = nullptr;
        bool m_originalIconsVisiblePreference = true;
    };
} // namespace Hammer
