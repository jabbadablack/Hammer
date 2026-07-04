#pragma once

#include <Clients/HammerSystemComponent.h>

#include <Hammer/HammerEditorViewportBus.h>

#include <AzToolsFramework/ActionManager/ActionManagerRegistrationNotificationBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/function/function_template.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#if !defined(Q_MOC_RUN)
#include <QPointer>
#endif

class QDockWidget;
class QStatusBar;
class QToolButton;
class QWidget;

namespace Hammer
{
    class HammerViewportLayoutWidget;

    class HammerEditorSystemComponent
        : public HammerSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
        , protected AzToolsFramework::ActionManagerRegistrationNotificationBus::Handler
        , protected HammerViewportRequestBus::Handler
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

        void SetViewportCount(int count) override;
        void ToggleMaximizeActiveViewport() override;

        void RegisterViewportPane();
        void EmbedViewportInCenter();
        void CreateViewportCountButtons();
        void DestroyViewportCountButtons();

        void PrepareEditorChrome();
        void RestoreEditorChrome();
        QWidget* EmbedViewportPaneAsCentralWidget(
            const char* paneName, const AZStd::function<QWidget*()>& expectedContentAccessor);
        void RestoreViewportPaneToDockWidget(QWidget* content);
        void ClosePane(const char* paneName);
        void RegisterHotkeyAction(
            const char* actionId, const char* name, const char* description, const char* category, const char* hotkey,
            AZStd::function<void()> callback);
        QStatusBar* GetMainWindowStatusBar() const;

        QPointer<HammerViewportLayoutWidget> m_viewportLayoutWidget;
        AZStd::vector<QPointer<QToolButton>> m_viewportCountButtons;
        QPointer<QDockWidget> m_paneDockWidget;
    };
} // namespace Hammer
