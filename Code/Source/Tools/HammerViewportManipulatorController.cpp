#include "HammerViewportManipulatorController.h"

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Input/Buses/Requests/InputSystemCursorRequestBus.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Viewport/ScreenGeometry.h>
#include <AzFramework/Viewport/ViewportScreen.h>
#include <AzToolsFramework/API/SettingsRegistryUtils.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>
#include <AzToolsFramework/ViewportSelection/EditorInteractionSystemViewportSelectionRequestBus.h>
#include <AzToolsFramework/ViewportSelection/EditorSelectionUtil.h>
#include <AzToolsFramework/Viewport/ViewportInteractionHelpers.h>

#include <QApplication>

namespace Hammer
{
    static const auto ManipulatorPriority = AzFramework::ViewportControllerPriority::Highest;
    static const auto InteractionPriority = AzFramework::ViewportControllerPriority::High;

    namespace
    {
        AzFramework::ViewportId g_activeHammerViewportId = AzFramework::InvalidViewportId;
    }

    void SetActiveHammerViewportId(AzFramework::ViewportId viewportId)
    {
        g_activeHammerViewportId = viewportId;
    }

    AzFramework::ViewportId GetActiveHammerViewportId()
    {
        return g_activeHammerViewportId;
    }

    HammerViewportManipulatorControllerInstance::HammerViewportManipulatorControllerInstance(
        AzFramework::ViewportId viewport, HammerViewportManipulatorController* controller)
        : AzFramework::MultiViewportControllerInstanceInterface<HammerViewportManipulatorController>(viewport, controller)
    {
        AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler::BusConnect(viewport);
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusConnect(viewport);
    }

    HammerViewportManipulatorControllerInstance::~HammerViewportManipulatorControllerInstance()
    {
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusDisconnect();
        AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler::BusDisconnect();
    }

    void HammerViewportManipulatorControllerInstance::FindVisibleEntities(AZStd::vector<AZ::EntityId>& visibleEntities)
    {
        visibleEntities.assign(m_entityVisibilityQuery.Begin(), m_entityVisibilityQuery.End());
    }

    // Registry keys/defaults below match Code/Editor/EditorViewportSettings.cpp exactly (Editor-private,
    // so Hammer can't call its SandboxEditor:: free functions directly, but reads the same Settings
    // Registry keys with the same defaults instead) - see the class comment on these overrides for why.
    bool HammerViewportManipulatorControllerInstance::GridSnappingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/GridSnapping", false);
    }

    float HammerViewportManipulatorControllerInstance::GridSize() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/GridSize", 0.1));
    }

    bool HammerViewportManipulatorControllerInstance::ShowGrid() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/ShowGrid", false);
    }

    bool HammerViewportManipulatorControllerInstance::AngleSnappingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/AngleSnapping", false);
    }

    float HammerViewportManipulatorControllerInstance::AngleStep() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/AngleSize", 5.0));
    }

    float HammerViewportManipulatorControllerInstance::ManipulatorLineBoundWidth() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Manipulator/LineBoundWidth", 0.1));
    }

    float HammerViewportManipulatorControllerInstance::ManipulatorCircleBoundWidth() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Manipulator/CircleBoundWidth", 0.1));
    }

    bool HammerViewportManipulatorControllerInstance::StickySelectEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/StickySelect", false);
    }

    AZ::Vector3 HammerViewportManipulatorControllerInstance::DefaultEditorCameraPosition() const
    {
        return AZ::Vector3(
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/x", 0.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/y", -10.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/z", 4.0)));
    }

    AZ::Vector2 HammerViewportManipulatorControllerInstance::DefaultEditorCameraOrientation() const
    {
        return AZ::Vector2(
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPitch", 0.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingYaw", 0.0)));
    }

    bool HammerViewportManipulatorControllerInstance::IconsVisible() const
    {
        // Restricted to just the active viewport, matching how gizmos and the ViewportUi overlay
        // already behave. Deliberately NOT ANDed with AzToolsFramework::IconsVisible() (the global
        // user preference) - HammerEditorSystemComponent forces that global setting to false (see
        // its comment for why: it's the only lever that suppresses the real, permanently-hidden
        // viewport's own unconditional per-frame icon submission, which - due to a confirmed engine
        // bug where AZ::RPI::DynamicDrawContext's RenderPipeline output scope is actually
        // implemented as Scene output scope - was rendering into every Hammer viewport regardless of
        // this restriction). Reading it here too would mean Hammer's own active viewport never shows
        // icons either. The tradeoff: toggling the Editor's own "show icons" preference no longer
        // has any effect on Hammer's viewports specifically.
        return GetViewportId() == GetActiveHammerViewportId();
    }

    bool HammerViewportManipulatorControllerInstance::HelpersVisible() const
    {
        return AzToolsFramework::HelpersVisible();
    }

    bool HammerViewportManipulatorControllerInstance::OnlyShowHelpersForSelectedEntities() const
    {
        return AzToolsFramework::OnlyShowHelpersForSelectedEntities();
    }

    bool HammerViewportManipulatorControllerInstance::HandleInputChannelEvent(const AzFramework::ViewportControllerInputEvent& event)
    {
        if (event.m_priority != ManipulatorPriority && event.m_priority != InteractionPriority)
        {
            return false;
        }

        using InteractionBus = AzToolsFramework::EditorInteractionSystemViewportSelectionRequestBus;
        using AzFramework::InputChannel;
        using AzToolsFramework::ViewportInteraction::KeyboardModifier;
        using AzToolsFramework::ViewportInteraction::MouseButton;
        using AzToolsFramework::ViewportInteraction::MouseEvent;
        using AzToolsFramework::ViewportInteraction::MouseInteraction;
        using AzToolsFramework::ViewportInteraction::MouseInteractionEvent;
        using AzToolsFramework::ViewportInteraction::ProjectedViewportRay;
        using AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus;
        using AzToolsFramework::ViewportInteraction::Helpers;

        bool interactionHandled = false;
        float wheelDelta = 0.0f;
        AZStd::optional<MouseButton> overrideButton;
        AZStd::optional<MouseEvent> eventType;

        const bool finishedProcessingEvents = event.m_priority == InteractionPriority;

        const auto state = event.m_inputChannel.GetState();
        if (Helpers::IsMouseMove(event.m_inputChannel))
        {
            if (event.m_priority == ManipulatorPriority)
            {
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

            eventType = MouseEvent::Move;
        }
        else if (auto mouseButton = Helpers::GetMouseButton(event.m_inputChannel); mouseButton != MouseButton::None)
        {
            const AZ::u32 mouseButtonValue = static_cast<AZ::u32>(mouseButton);
            overrideButton = mouseButton;
            if (state == InputChannel::State::Began)
            {
                m_mouseInteraction.m_mouseButtons.m_mouseButtons |= mouseButtonValue;
                if (IsDoubleClick(mouseButton))
                {
                    if (event.m_priority == InteractionPriority)
                    {
                        m_pendingDoubleClicks.erase(mouseButton);
                    }
                    eventType = MouseEvent::DoubleClick;
                }
                else
                {
                    if (finishedProcessingEvents)
                    {
                        m_pendingDoubleClicks[mouseButton] = { m_currentTime, m_mouseInteraction.m_mousePick.m_screenCoordinates };
                    }
                    eventType = MouseEvent::Down;
                }
            }
            else if (state == InputChannel::State::Ended)
            {
                if (m_mouseInteraction.m_mouseButtons.m_mouseButtons & mouseButtonValue)
                {
                    if (event.m_priority == InteractionPriority)
                    {
                        m_mouseInteraction.m_mouseButtons.m_mouseButtons &= ~mouseButtonValue;
                    }
                    eventType = MouseEvent::Up;
                }
            }
        }
        else if (auto keyboardModifier = Helpers::GetKeyboardModifier(event.m_inputChannel); keyboardModifier != KeyboardModifier::None)
        {
            if (state == InputChannel::State::Began || state == InputChannel::State::Updated)
            {
                m_mouseInteraction.m_keyboardModifiers.m_keyModifiers |= static_cast<AZ::u32>(keyboardModifier);
            }
            else if (state == InputChannel::State::Ended)
            {
                m_mouseInteraction.m_keyboardModifiers.m_keyModifiers &= ~static_cast<AZ::u32>(keyboardModifier);
            }
        }
        else if (event.m_inputChannel.GetInputChannelId() == AzFramework::InputDeviceMouse::Movement::Z)
        {
            if (state == InputChannel::State::Began || state == InputChannel::State::Updated)
            {
                eventType = MouseEvent::Wheel;
                wheelDelta = event.m_inputChannel.GetValue();
            }
        }

        if (eventType == MouseEvent::Down || eventType == MouseEvent::DoubleClick)
        {
            // Guarantees the shared pick cache is freshly rebuilt (from this viewport's own
            // FindVisibleEntities()/m_entityVisibilityQuery) immediately before the click reaches
            // the interaction bus's hit-test, rather than relying on however recently the last
            // per-tick UpdateViewport() refresh happened to run.
            RefreshEntityVisibilityCache();
        }

        if (eventType)
        {
            MouseInteraction mouseInteraction = m_mouseInteraction;
            if (overrideButton)
            {
                mouseInteraction.m_mouseButtons.m_mouseButtons = static_cast<AZ::u32>(overrideButton.value());
            }

            mouseInteraction.m_interactionId.m_viewportId = GetViewportId();

            const auto& targetInteractionEvent = event.m_priority == ManipulatorPriority
                ? &InteractionBus::Events::InternalHandleMouseManipulatorInteraction
                : &InteractionBus::Events::InternalHandleMouseViewportInteraction;

            auto currentCursorState = AzFramework::SystemCursorState::Unknown;
            AzFramework::InputSystemCursorRequestBus::EventResult(
                currentCursorState, event.m_inputChannel.GetInputDevice().GetInputDeviceId(),
                &AzFramework::InputSystemCursorRequestBus::Events::GetSystemCursorState);

            const auto mouseInteractionEvent = [mouseInteraction, event = eventType.value(), wheelDelta,
                                                cursorCaptured = currentCursorState == AzFramework::SystemCursorState::ConstrainedAndHidden]
            {
                switch (event)
                {
                case MouseEvent::Up:
                case MouseEvent::Down:
                case MouseEvent::Move:
                case MouseEvent::DoubleClick:
                    return MouseInteractionEvent(AZStd::move(mouseInteraction), event, cursorCaptured);
                case MouseEvent::Wheel:
                    return MouseInteractionEvent(AZStd::move(mouseInteraction), wheelDelta);
                }

                AZ_Assert(false, "Unhandled MouseEvent");
                return MouseInteractionEvent(MouseInteraction{}, MouseEvent::Up, false);
            }();

            InteractionBus::EventResult(
                interactionHandled, AzToolsFramework::GetEntityContextId(), targetInteractionEvent, mouseInteractionEvent);
        }

        return interactionHandled;
    }

    void HammerViewportManipulatorControllerInstance::ResetInputChannels()
    {
        m_pendingDoubleClicks.clear();
        m_mouseInteraction = AzToolsFramework::ViewportInteraction::MouseInteraction();
    }

    void HammerViewportManipulatorControllerInstance::UpdateViewport(const AzFramework::ViewportControllerUpdateEvent& event)
    {
        m_currentTime = event.m_time;

        // Mirrors Code/Editor/EditorViewportWidget.cpp's own m_entityVisibilityQuery.UpdateVisibility()
        // call in its Update() - the same AzFramework::EntityVisibilityQuery class, fully public.
        // This is what FindVisibleEntities() (below) answers from.
        AzFramework::CameraState cameraState;
        AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::EventResult(
            cameraState, GetViewportId(), &AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::Events::GetCameraState);
        m_entityVisibilityQuery.UpdateVisibility(cameraState);

        RefreshEntityVisibilityCache();
    }

    void HammerViewportManipulatorControllerInstance::RefreshEntityVisibilityCache() const
    {
        // EditorInteractionSystemComponent::DisplayViewport (Code/Framework/AzToolsFramework/.../
        // EditorInteractionSystemComponent.cpp) is the single, fully public handler that both
        // populates the visible-entity pick cache used for selection (via this instance's
        // FindVisibleEntities(), see above) and draws the shared ManipulatorManager's gizmos - it
        // does all of this internally once this event fires for a given viewport, so no direct
        // access to the manipulator manager is needed here.
        AzFramework::DebugDisplayRequestBus::BusPtr debugDisplayBus;
        AzFramework::DebugDisplayRequestBus::Bind(debugDisplayBus, GetViewportId());
        if (AzFramework::DebugDisplayRequests* debugDisplay = AzFramework::DebugDisplayRequestBus::FindFirstHandler(debugDisplayBus))
        {
            AzFramework::ViewportDebugDisplayEventBus::Event(
                AzToolsFramework::GetEntityContextId(), &AzFramework::ViewportDebugDisplayEvents::DisplayViewport,
                AzFramework::ViewportInfo{ GetViewportId() }, *debugDisplay);
        }
    }

    bool HammerViewportManipulatorControllerInstance::IsDoubleClick(AzToolsFramework::ViewportInteraction::MouseButton button) const
    {
        if (auto clickIt = m_pendingDoubleClicks.find(button); clickIt != m_pendingDoubleClicks.end())
        {
            const double doubleClickThresholdMilliseconds = qApp->doubleClickInterval();
            const bool insideTimeThreshold =
                (m_currentTime.GetMilliseconds() - clickIt->second.m_time.GetMilliseconds()) < doubleClickThresholdMilliseconds;
            const bool insideDistanceThreshold =
                AzFramework::ScreenVectorLength(clickIt->second.m_position - m_mouseInteraction.m_mousePick.m_screenCoordinates) <
                AzFramework::DefaultMouseMoveDeadZone;
            return insideTimeThreshold && insideDistanceThreshold;
        }

        return false;
    }
}
