#pragma once

#if !defined(Q_MOC_RUN)
#include <QPointer>
#endif

#include <Clients/HammerSystemComponent.h>
#include <Hammer/HammerEditorViewportBus.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

class QWidget;

namespace Hammer
{
    class ViewportLayout;

    class EditorSystemComponent
        : public SystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = SystemComponent;
    public:
        AZ_COMPONENT_DECL(EditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;
        void Deactivate() override;

        void NotifyCentralWidgetInitialized() override;
        void NotifyEditorInitialized() override;

        void EmbedViewportInCenter();

        QPointer<ViewportLayout> m_viewportLayout;
        bool m_seedHammerLayout = false;
    };
} // namespace Hammer
