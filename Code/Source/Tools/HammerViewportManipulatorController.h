#pragma once

#include <AzCore/Script/ScriptTimePoint.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Viewport/MultiViewportController.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzToolsFramework/Viewport/ViewportTypes.h>

namespace Hammer
{
    class HammerViewportManipulatorControllerInstance;

    // Routes raw input to AzToolsFramework's manipulator/selection system, giving a viewport the
    // same entity selection and gizmo manipulation as the Editor's default viewport.
    // Mirrors SandboxEditor::ViewportManipulatorController (Code/Editor/ViewportManipulatorController.h),
    // which is Editor-private, using only public AzToolsFramework/AzFramework APIs.
    using HammerViewportManipulatorController = AzFramework::MultiViewportController<
        HammerViewportManipulatorControllerInstance, AzFramework::ViewportControllerPriority::DispatchToAllPriorities>;

    class HammerViewportManipulatorControllerInstance final
        : public AzFramework::MultiViewportControllerInstanceInterface<HammerViewportManipulatorController>
    {
    public:
        HammerViewportManipulatorControllerInstance(AzFramework::ViewportId viewport, HammerViewportManipulatorController* controller);
        ~HammerViewportManipulatorControllerInstance() = default;

        bool HandleInputChannelEvent(const AzFramework::ViewportControllerInputEvent& event) override;
        void ResetInputChannels() override;
        void UpdateViewport(const AzFramework::ViewportControllerUpdateEvent& event) override;

    private:
        bool IsDoubleClick(AzToolsFramework::ViewportInteraction::MouseButton) const;

        struct ClickEvent
        {
            AZ::ScriptTimePoint m_time;
            AzFramework::ScreenPoint m_position;
        };

        AzToolsFramework::ViewportInteraction::MouseInteraction m_mouseInteraction;
        AZStd::unordered_map<AzToolsFramework::ViewportInteraction::MouseButton, ClickEvent> m_pendingDoubleClicks;
        AZ::ScriptTimePoint m_currentTime;
    };
}
