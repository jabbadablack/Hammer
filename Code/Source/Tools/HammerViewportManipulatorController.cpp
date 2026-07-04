#include "HammerViewportManipulatorController.h"
#include "HammerEditorViewportSettings.h"
#include "HammerQtEnvironment.h"

#include <AzCore/std/containers/array.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Input/Buses/Requests/InputSystemCursorRequestBus.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Viewport/ScreenGeometry.h>
#include <AzFramework/Viewport/ViewportScreen.h>
#include <AzToolsFramework/Viewport/ViewportInteractionHelpers.h>
#include <AzToolsFramework/ViewportSelection/EditorInteractionSystemViewportSelectionRequestBus.h>
#include <AzToolsFramework/ViewportSelection/EditorSelectionUtil.h>

namespace Hammer
{
    static const auto ManipulatorPriority = AzFramework::ViewportControllerPriority::Highest;
    static const auto InteractionPriority = AzFramework::ViewportControllerPriority::High;

    HammerViewportManipulatorControllerInstance::HammerViewportManipulatorControllerInstance(
        AzFramework::ViewportId viewport, HammerViewportManipulatorController* controller)
        : AzFramework::MultiViewportControllerInstanceInterface<HammerViewportManipulatorController>(viewport, controller)
        , m_viewportSettings(AZStd::make_unique<HammerEditorViewportSettings>(viewport))
    {
        AZ_Assert(controller, "HammerViewportManipulatorControllerInstance constructed with a null controller");
        AZ_Assert(viewport != AzFramework::InvalidViewportId, "HammerViewportManipulatorControllerInstance constructed with an invalid ViewportId");
        AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler::BusConnect(viewport);
    }

    HammerViewportManipulatorControllerInstance::~HammerViewportManipulatorControllerInstance()
    {
        AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler::BusDisconnect();
    }

    void HammerViewportManipulatorControllerInstance::FindVisibleEntities(AZStd::vector<AZ::EntityId>& visibleEntities)
    {
        visibleEntities.assign(m_entityVisibilityQuery.Begin(), m_entityVisibilityQuery.End());
    }

    bool HammerViewportManipulatorControllerInstance::HandleInputChannelEvent(const AzFramework::ViewportControllerInputEvent& event)
    {
        bool interactionHandled = false;
        const bool relevantPriority = event.m_priority == ManipulatorPriority || event.m_priority == InteractionPriority;
        relevantPriority && (ProcessRelevantInputEvent(event, interactionHandled), true);
        return interactionHandled;
    }

    void HammerViewportManipulatorControllerInstance::ProcessRelevantInputEvent(
        const AzFramework::ViewportControllerInputEvent& event, bool& interactionHandled)
    {
        AZ_Assert(
            event.m_priority == ManipulatorPriority || event.m_priority == InteractionPriority,
            "ProcessRelevantInputEvent called with an unexpected controller priority");
        AZ_Assert(!interactionHandled, "ProcessRelevantInputEvent called with interactionHandled already set");

        using AzToolsFramework::ViewportInteraction::Helpers;
        using AzToolsFramework::ViewportInteraction::KeyboardModifier;
        using AzToolsFramework::ViewportInteraction::MouseButton;
        using AzToolsFramework::ViewportInteraction::MouseEvent;

        float wheelDelta = 0.0f;
        MouseButton overrideButton = MouseButton::None;
        MouseEvent eventType = MouseEvent::Up;
        bool eventTypeSet = false;

        const bool finishedProcessingEvents = event.m_priority == InteractionPriority;

        bool classified = false;

        const bool isMouseMove = Helpers::IsMouseMove(event.m_inputChannel);
        (!classified && isMouseMove) && (ClassifyMouseMove(event, eventType, eventTypeSet), classified = true, true);

        const MouseButton mouseButton = Helpers::GetMouseButton(event.m_inputChannel);
        (!classified && mouseButton != MouseButton::None) &&
            (ClassifyMouseButton(event, mouseButton, finishedProcessingEvents, eventType, eventTypeSet, overrideButton),
             classified = true, true);

        const KeyboardModifier keyboardModifier = Helpers::GetKeyboardModifier(event.m_inputChannel);
        (!classified && keyboardModifier != KeyboardModifier::None) &&
            (ClassifyKeyboardModifier(event, keyboardModifier), classified = true, true);

        (!classified && event.m_inputChannel.GetInputChannelId() == AzFramework::InputDeviceMouse::Movement::Z) &&
            (ClassifyWheel(event, eventType, eventTypeSet, wheelDelta), classified = true, true);

        const bool isDownOrDoubleClick = eventTypeSet && (eventType == MouseEvent::Down || eventType == MouseEvent::DoubleClick);
        isDownOrDoubleClick && (RefreshEntityVisibilityCache(), true);

        eventTypeSet &&
            (DispatchMouseInteractionEvent(event, eventType, wheelDelta, overrideButton, interactionHandled), true);
    }

    void HammerViewportManipulatorControllerInstance::ClassifyMouseMove(
        const AzFramework::ViewportControllerInputEvent& event,
        AzToolsFramework::ViewportInteraction::MouseEvent& eventType, bool& eventTypeSet)
    {
        (event.m_priority == ManipulatorPriority) && (UpdateMousePick(event), true);
        eventType = AzToolsFramework::ViewportInteraction::MouseEvent::Move;
        eventTypeSet = true;
    }

    void HammerViewportManipulatorControllerInstance::UpdateMousePick(const AzFramework::ViewportControllerInputEvent& event)
    {
        using AzToolsFramework::ViewportInteraction::ProjectedViewportRay;
        using AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus;

        const auto* position = event.m_inputChannel.GetCustomData<AzFramework::InputChannel::PositionData2D>();
        AZ_Assert(position, "Expected PositionData2D but found nullptr");

        AzFramework::WindowSize windowSize;
        AzFramework::WindowRequestBus::EventResult(
            windowSize, event.m_windowHandle, &AzFramework::WindowRequestBus::Events::GetRenderResolution);

        const auto screenPoint = AzFramework::ScreenPoint(
            aznumeric_cast<int>(position->m_normalizedPosition.GetX() * windowSize.m_width),
            aznumeric_cast<int>(position->m_normalizedPosition.GetY() * windowSize.m_height));

        ProjectedViewportRay ray{};
        ViewportInteractionRequestBus::EventResult(
            ray, GetViewportId(), &ViewportInteractionRequestBus::Events::ViewportScreenToWorldRay, screenPoint);

        m_mouseInteraction.m_mousePick.m_rayOrigin = ray.m_origin;
        m_mouseInteraction.m_mousePick.m_rayDirection = ray.m_direction;
        m_mouseInteraction.m_mousePick.m_screenCoordinates = screenPoint;
    }

    void HammerViewportManipulatorControllerInstance::ClassifyMouseButton(
        const AzFramework::ViewportControllerInputEvent& event, AzToolsFramework::ViewportInteraction::MouseButton mouseButton,
        bool finishedProcessingEvents, AzToolsFramework::ViewportInteraction::MouseEvent& eventType, bool& eventTypeSet,
        AzToolsFramework::ViewportInteraction::MouseButton& overrideButton)
    {
        using AzFramework::InputChannel;
        using AzToolsFramework::ViewportInteraction::MouseButton;

        AZ_Assert(mouseButton != MouseButton::None, "ClassifyMouseButton called with MouseButton::None");

        const AZ::u32 mouseButtonValue = static_cast<AZ::u32>(mouseButton);
        overrideButton = mouseButton;

        const auto state = event.m_inputChannel.GetState();

        (state == InputChannel::State::Began) &&
            (OnMouseButtonBegan(mouseButton, mouseButtonValue, finishedProcessingEvents, eventType, eventTypeSet), true);

        (state == InputChannel::State::Ended && (m_mouseInteraction.m_mouseButtons.m_mouseButtons & mouseButtonValue)) &&
            (OnMouseButtonEnded(mouseButtonValue, finishedProcessingEvents, eventType, eventTypeSet), true);
    }

    void HammerViewportManipulatorControllerInstance::OnMouseButtonBegan(
        AzToolsFramework::ViewportInteraction::MouseButton mouseButton, AZ::u32 mouseButtonValue, bool finishedProcessingEvents,
        AzToolsFramework::ViewportInteraction::MouseEvent& eventType, bool& eventTypeSet)
    {
        using AzToolsFramework::ViewportInteraction::MouseEvent;

        m_mouseInteraction.m_mouseButtons.m_mouseButtons |= mouseButtonValue;

        const bool isDoubleClick = IsDoubleClick(mouseButton);
        constexpr AZStd::array<MouseEvent, 2> DownEvents = { MouseEvent::Down, MouseEvent::DoubleClick };
        eventType = DownEvents[static_cast<size_t>(isDoubleClick)];
        eventTypeSet = true;

        finishedProcessingEvents && (UpdatePendingDoubleClick(mouseButton, isDoubleClick), true);
    }

    void HammerViewportManipulatorControllerInstance::UpdatePendingDoubleClick(
        AzToolsFramework::ViewportInteraction::MouseButton mouseButton, bool isDoubleClick)
    {
        const AZStd::array<AZStd::function<void()>, 2> actions = {
            [this, mouseButton]
            {
                m_pendingDoubleClicks[mouseButton] = { m_currentTime, m_mouseInteraction.m_mousePick.m_screenCoordinates };
            },
            [this, mouseButton]
            {
                m_pendingDoubleClicks.erase(mouseButton);
            },
        };
        actions[static_cast<size_t>(isDoubleClick)]();
    }

    void HammerViewportManipulatorControllerInstance::OnMouseButtonEnded(
        AZ::u32 mouseButtonValue, bool finishedProcessingEvents,
        AzToolsFramework::ViewportInteraction::MouseEvent& eventType, bool& eventTypeSet)
    {
        m_mouseInteraction.m_mouseButtons.m_mouseButtons &= ~(mouseButtonValue * static_cast<AZ::u32>(finishedProcessingEvents));
        eventType = AzToolsFramework::ViewportInteraction::MouseEvent::Up;
        eventTypeSet = true;
    }

    void HammerViewportManipulatorControllerInstance::ClassifyKeyboardModifier(
        const AzFramework::ViewportControllerInputEvent& event, AzToolsFramework::ViewportInteraction::KeyboardModifier keyboardModifier)
    {
        using AzFramework::InputChannel;
        using AzToolsFramework::ViewportInteraction::KeyboardModifier;

        AZ_Assert(keyboardModifier != KeyboardModifier::None, "ClassifyKeyboardModifier called with KeyboardModifier::None");

        const AZ::u32 modifierBit = static_cast<AZ::u32>(keyboardModifier);
        const auto state = event.m_inputChannel.GetState();
        const bool modifierActive = state == InputChannel::State::Began || state == InputChannel::State::Updated;
        const bool modifierEnded = state == InputChannel::State::Ended;

        m_mouseInteraction.m_keyboardModifiers.m_keyModifiers =
            (m_mouseInteraction.m_keyboardModifiers.m_keyModifiers | (modifierBit * static_cast<AZ::u32>(modifierActive))) &
            ~(modifierBit * static_cast<AZ::u32>(modifierEnded));
    }

    void HammerViewportManipulatorControllerInstance::ClassifyWheel(
        const AzFramework::ViewportControllerInputEvent& event,
        AzToolsFramework::ViewportInteraction::MouseEvent& eventType, bool& eventTypeSet, float& wheelDelta)
    {
        using AzFramework::InputChannel;

        const auto state = event.m_inputChannel.GetState();
        const bool active = state == InputChannel::State::Began || state == InputChannel::State::Updated;

        active && (eventType = AzToolsFramework::ViewportInteraction::MouseEvent::Wheel, eventTypeSet = true, true);
        active && (wheelDelta = event.m_inputChannel.GetValue(), true);
    }

    void HammerViewportManipulatorControllerInstance::DispatchMouseInteractionEvent(
        const AzFramework::ViewportControllerInputEvent& event, AzToolsFramework::ViewportInteraction::MouseEvent eventType,
        float wheelDelta, AzToolsFramework::ViewportInteraction::MouseButton overrideButton, bool& interactionHandled)
    {
        using InteractionBus = AzToolsFramework::EditorInteractionSystemViewportSelectionRequestBus;
        using AzToolsFramework::ViewportInteraction::MouseButton;
        using AzToolsFramework::ViewportInteraction::MouseInteraction;
        using AzToolsFramework::ViewportInteraction::MouseInteractionEvent;

        AZ_Assert(
            event.m_priority == ManipulatorPriority || event.m_priority == InteractionPriority,
            "DispatchMouseInteractionEvent called with an unexpected controller priority");
        AZ_Assert(!interactionHandled, "DispatchMouseInteractionEvent called with interactionHandled already set");

        MouseInteraction mouseInteraction = m_mouseInteraction;
        (overrideButton != MouseButton::None) &&
            (mouseInteraction.m_mouseButtons.m_mouseButtons = static_cast<AZ::u32>(overrideButton), true);
        mouseInteraction.m_interactionId.m_viewportId = GetViewportId();

        using TargetMemberPtr = decltype(&InteractionBus::Events::InternalHandleMouseManipulatorInteraction);
        const AZStd::array<TargetMemberPtr, 2> targets = { &InteractionBus::Events::InternalHandleMouseViewportInteraction,
                                                            &InteractionBus::Events::InternalHandleMouseManipulatorInteraction };
        const auto targetInteractionEvent = targets[static_cast<size_t>(event.m_priority == ManipulatorPriority)];

        auto currentCursorState = AzFramework::SystemCursorState::Unknown;
        AzFramework::InputSystemCursorRequestBus::EventResult(
            currentCursorState, event.m_inputChannel.GetInputDevice().GetInputDeviceId(),
            &AzFramework::InputSystemCursorRequestBus::Events::GetSystemCursorState);
        const bool cursorCaptured = currentCursorState == AzFramework::SystemCursorState::ConstrainedAndHidden;

        const bool isWheel = eventType == AzToolsFramework::ViewportInteraction::MouseEvent::Wheel;
        const AZStd::array<MouseInteractionEvent, 2> candidateEvents = {
            MouseInteractionEvent(mouseInteraction, eventType, cursorCaptured),
            MouseInteractionEvent(mouseInteraction, wheelDelta),
        };
        const MouseInteractionEvent& mouseInteractionEvent = candidateEvents[static_cast<size_t>(isWheel)];

        InteractionBus::EventResult(interactionHandled, AzToolsFramework::GetEntityContextId(), targetInteractionEvent, mouseInteractionEvent);
    }

    void HammerViewportManipulatorControllerInstance::ResetInputChannels()
    {
        m_pendingDoubleClicks.clear();
        m_mouseInteraction = AzToolsFramework::ViewportInteraction::MouseInteraction();
    }

    void HammerViewportManipulatorControllerInstance::UpdateViewport(const AzFramework::ViewportControllerUpdateEvent& event)
    {
        m_currentTime = event.m_time;

        AzFramework::CameraState cameraState;
        AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::EventResult(
            cameraState, GetViewportId(), &AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::Events::GetCameraState);
        m_entityVisibilityQuery.UpdateVisibility(cameraState);

        RefreshEntityVisibilityCache();
    }

    void HammerViewportManipulatorControllerInstance::RefreshEntityVisibilityCache() const
    {
        AzFramework::DebugDisplayRequestBus::BusPtr debugDisplayBus;
        AzFramework::DebugDisplayRequestBus::Bind(debugDisplayBus, GetViewportId());
        AzFramework::DebugDisplayRequests* debugDisplay = AzFramework::DebugDisplayRequestBus::FindFirstHandler(debugDisplayBus);
        debugDisplay &&
            (AzFramework::ViewportDebugDisplayEventBus::Event(
                 AzToolsFramework::GetEntityContextId(), &AzFramework::ViewportDebugDisplayEvents::DisplayViewport,
                 AzFramework::ViewportInfo{ GetViewportId() }, *debugDisplay),
             true);
    }

    bool HammerViewportManipulatorControllerInstance::IsDoubleClick(AzToolsFramework::ViewportInteraction::MouseButton button) const
    {
        const auto clickIt = m_pendingDoubleClicks.find(button);
        const bool found = clickIt != m_pendingDoubleClicks.end();

        bool insideTimeThreshold = false;
        bool insideDistanceThreshold = false;
        found &&
            (insideTimeThreshold =
                 (m_currentTime.GetMilliseconds() - clickIt->second.m_time.GetMilliseconds()) < DoubleClickIntervalMs(),
             insideDistanceThreshold =
                 AzFramework::ScreenVectorLength(clickIt->second.m_position - m_mouseInteraction.m_mousePick.m_screenCoordinates) <
                 AzFramework::DefaultMouseMoveDeadZone,
             true);

        return found && insideTimeThreshold && insideDistanceThreshold;
    }
}
