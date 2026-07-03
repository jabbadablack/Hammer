#pragma once

#include <Clients/HammerSystemComponent.h>

#include <AzToolsFramework/ActionManager/ActionManagerRegistrationNotificationBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include "HammerViewportLayoutHandle.h"

#if !defined(Q_MOC_RUN)
#include <QPointer>
#endif

class QToolButton;

namespace Hammer
{
    class HammerViewportLayoutWidget;
    class HammerAdapterRegistry;

    class HammerEditorSystemComponent
        : public HammerSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
        , protected AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler
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

        void OnActionRegistrationHook() override;

        void RegisterViewportPane();
        void EmbedViewportInCenter();
        void CreateViewportCountButtons();
        void DestroyViewportCountButtons();

        AZStd::unique_ptr<HammerAdapterRegistry> m_adapters;
        AZStd::unique_ptr<IHammerViewportLayoutHandle> m_viewportLayoutHandle = AZStd::make_unique<NullViewportLayoutHandle>();
        QPointer<HammerViewportLayoutWidget> m_viewportLayoutWidget;
        AZStd::vector<QPointer<QToolButton>> m_viewportCountButtons;
    };
} // namespace Hammer
