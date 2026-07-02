#pragma once

#include <AzCore/Script/ScriptTimePoint.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Viewport/MultiViewportController.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzFramework/Visibility/EntityVisibilityQuery.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
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
        , public AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler
    {
    public:
        HammerViewportManipulatorControllerInstance(AzFramework::ViewportId viewport, HammerViewportManipulatorController* controller);
        ~HammerViewportManipulatorControllerInstance() override;

        bool HandleInputChannelEvent(const AzFramework::ViewportControllerInputEvent& event) override;
        void ResetInputChannels() override;
        void UpdateViewport(const AzFramework::ViewportControllerUpdateEvent& event) override;

        // AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler
        // Answers AzToolsFramework::EditorVisibleEntityDataCache's query for which entities are
        // visible in this viewport - see the comment on RefreshEntityVisibilityCache() for why this
        // is required (this bus has no handler at all otherwise, for any Hammer viewport).
        void FindVisibleEntities(AZStd::vector<AZ::EntityId>& visibleEntities) override;

    private:
        bool IsDoubleClick(AzToolsFramework::ViewportInteraction::MouseButton) const;

        // Broadcasts DisplayViewport for this viewport, which (among other things) rebuilds
        // AzToolsFramework's shared entity-visibility/pick cache. Called every tick from
        // UpdateViewport(), and also synchronously right before forwarding a click to the
        // interaction bus - see the comment on this function's definition for why.
        void RefreshEntityVisibilityCache() const;

        struct ClickEvent
        {
            AZ::ScriptTimePoint m_time;
            AzFramework::ScreenPoint m_position;
        };

        AzToolsFramework::ViewportInteraction::MouseInteraction m_mouseInteraction;
        AZStd::unordered_map<AzToolsFramework::ViewportInteraction::MouseButton, ClickEvent> m_pendingDoubleClicks;
        AZ::ScriptTimePoint m_currentTime;

        // Drives FindVisibleEntities() - refreshed every tick in UpdateViewport() from this
        // viewport's own camera state. Mirrors exactly what Code/Editor/EditorViewportWidget.cpp's
        // m_entityVisibilityQuery does (the same AzFramework::EntityVisibilityQuery class, fully
        // public), which is the piece Hammer's viewports were missing entirely - without it,
        // AzToolsFramework::EditorVisibleEntityDataCache's FindVisibleEntities broadcast had no
        // handler for any Hammer viewport ID, so it silently returned zero entities always,
        // regardless of camera position - not a race condition, and not caused by the camera
        // controller.
        AzFramework::EntityVisibilityQuery m_entityVisibilityQuery;
    };
}
