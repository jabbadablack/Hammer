#pragma once

#include <AzCore/Script/ScriptTimePoint.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Viewport/MultiViewportController.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzFramework/Visibility/EntityVisibilityQuery.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
#include <AzToolsFramework/Viewport/ViewportTypes.h>

namespace Hammer
{
    class HammerViewportManipulatorControllerInstance;
    class HammerEditorViewportSettings;

    class HammerViewportManipulatorController final
        : public AzFramework::MultiViewportController<
              HammerViewportManipulatorControllerInstance, AzFramework::ViewportControllerPriority::DispatchToAllPriorities>
    {
    };

    class HammerViewportManipulatorControllerInstance final
        : public AzFramework::MultiViewportControllerInstanceInterface<HammerViewportManipulatorController>
        , public AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler
    {
    public:
        HammerViewportManipulatorControllerInstance(AzFramework::ViewportId viewport, HammerViewportManipulatorController* controller);
        ~HammerViewportManipulatorControllerInstance() override;

        bool HandleInputChannelEvent(const AzFramework::ViewportControllerInputEvent& event) override;
        void ResetInputChannels() override;
        void UpdateViewport(const AzFramework::ViewportControllerUpdateEvent& event) override;

        void FindVisibleEntities(AZStd::vector<AZ::EntityId>& visibleEntities) override;

    private:
        void ProcessRelevantInputEvent(const AzFramework::ViewportControllerInputEvent& event, bool& interactionHandled);

        void ClassifyMouseMove(
            const AzFramework::ViewportControllerInputEvent& event,
            AZStd::optional<AzToolsFramework::ViewportInteraction::MouseEvent>& eventType);
        void ClassifyMouseButton(
            const AzFramework::ViewportControllerInputEvent& event, AzToolsFramework::ViewportInteraction::MouseButton mouseButton,
            bool finishedProcessingEvents, AZStd::optional<AzToolsFramework::ViewportInteraction::MouseEvent>& eventType,
            AZStd::optional<AzToolsFramework::ViewportInteraction::MouseButton>& overrideButton);
        void ClassifyKeyboardModifier(
            const AzFramework::ViewportControllerInputEvent& event, AzToolsFramework::ViewportInteraction::KeyboardModifier keyboardModifier);
        void ClassifyWheel(
            const AzFramework::ViewportControllerInputEvent& event,
            AZStd::optional<AzToolsFramework::ViewportInteraction::MouseEvent>& eventType, float& wheelDelta);

        void UpdateMousePick(const AzFramework::ViewportControllerInputEvent& event);
        void OnMouseButtonBegan(
            AzToolsFramework::ViewportInteraction::MouseButton mouseButton, AZ::u32 mouseButtonValue, bool finishedProcessingEvents,
            AZStd::optional<AzToolsFramework::ViewportInteraction::MouseEvent>& eventType);
        void OnMouseButtonEnded(
            AZ::u32 mouseButtonValue, bool finishedProcessingEvents,
            AZStd::optional<AzToolsFramework::ViewportInteraction::MouseEvent>& eventType);
        void UpdatePendingDoubleClick(AzToolsFramework::ViewportInteraction::MouseButton mouseButton, bool isDoubleClick);

        void DispatchMouseInteractionEvent(
            const AzFramework::ViewportControllerInputEvent& event, AzToolsFramework::ViewportInteraction::MouseEvent eventType,
            float wheelDelta, const AZStd::optional<AzToolsFramework::ViewportInteraction::MouseButton>& overrideButton,
            bool& interactionHandled);

        bool IsDoubleClick(AzToolsFramework::ViewportInteraction::MouseButton) const;
        void RefreshEntityVisibilityCache() const;

        struct ClickEvent
        {
            AZ::ScriptTimePoint m_time;
            AzFramework::ScreenPoint m_position;
        };

        AzToolsFramework::ViewportInteraction::MouseInteraction m_mouseInteraction;
        AZStd::unordered_map<AzToolsFramework::ViewportInteraction::MouseButton, ClickEvent> m_pendingDoubleClicks;
        AZ::ScriptTimePoint m_currentTime;
        AzFramework::EntityVisibilityQuery m_entityVisibilityQuery;
        AZStd::unique_ptr<HammerEditorViewportSettings> m_viewportSettings;
    };
}
