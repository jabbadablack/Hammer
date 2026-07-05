#pragma once

#if !defined(Q_MOC_RUN)
#include <QPointer>
#endif

#include <Clients/HammerSystemComponent.h>
#include <Hammer/HammerEditorViewportBus.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/function/function_template.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ActionManager/ActionManagerRegistrationNotificationBus.h>

class QAction;
class QDockWidget;
class QToolButton;
class QWidget;

namespace Hammer
{
    class HammerViewportLayoutWidget;

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

        void NotifyRegisterViews() override;
        void NotifyEditorInitialized() override;

        void OnActionRegistrationHook() override;
        void OnWidgetActionRegistrationHook() override;
        void OnToolBarBindingHook() override;

        void RegisterViewportPane();
        void EmbedViewportInCenter();
        void CreateViewportCountButtons();
        void DestroyViewportCountButtons();
        QWidget* CreateViewModeToolBarButton();
        void ConnectViewModeSwitcherSync();
        void PrepareEditorChrome();
        QWidget* EmbedViewportPaneAsCentralWidget();
        void RestoreViewportPaneToDockWidget(QWidget* content);
        void RegisterHotkeyAction(
            const char* actionId, const char* name, const char* description, const char* hotkey, AZStd::function<void()> callback);

        QPointer<HammerViewportLayoutWidget> m_viewportLayoutWidget;
        AZStd::vector<QPointer<QToolButton>> m_viewportCountButtons;
        AZStd::vector<QPointer<QToolButton>> m_viewModeButtons;
        QPointer<QDockWidget> m_paneDockWidget;
    };
} // namespace Hammer
