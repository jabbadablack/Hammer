#pragma once

#if !defined(Q_MOC_RUN)
#include <QPointer>
#endif

#include <Clients/HammerSystemComponent.h>
#include <Hammer/HammerEditorViewportBus.h>

#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ActionManager/ActionManagerRegistrationNotificationBus.h>

class QToolButton;
class QWidget;

namespace Hammer
{
    class HammerViewportWidget;

    class HammerEditorSystemComponent
        : public HammerSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
        , protected AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler
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

        void OnWidgetActionRegistrationHook() override;

        void EmbedViewportInCenter();
        QWidget* CreateViewModeToolBarButton();
        void ConnectViewModeSwitcherSync();

        QPointer<HammerViewportWidget> m_viewportWidget;
        AZStd::vector<QPointer<QToolButton>> m_viewModeButtons;
        bool m_seedHammerLayout = false;
    };
} // namespace Hammer
