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
    class HammerViewportWidget;

    class HammerEditorSystemComponent
        : public HammerSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = HammerSystemComponent;
    public:
        AZ_COMPONENT_DECL(HammerEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        void Activate() override;
        void Deactivate() override;

        void NotifyCentralWidgetInitialized() override;
        void NotifyEditorInitialized() override;

        void EmbedViewportInCenter();

        QPointer<HammerViewportWidget> m_viewportWidget;
        bool m_seedHammerLayout = false;
    };
} // namespace Hammer
