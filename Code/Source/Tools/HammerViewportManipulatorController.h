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

    // Tracks whichever Hammer viewport is currently "active" (see HammerWidget::SetActive) - set by
    // HammerWidget::ApplyActiveState() whenever a viewport becomes active, read by
    // HammerViewportManipulatorControllerInstance::IconsVisible() so entity icons only render in the
    // active viewport. A free function rather than a per-instance flag since exactly one Hammer
    // viewport is ever active at a time by construction (see HammerWidget's class comment) - no
    // instance needs to know about any other instance's state, just this single shared value.
    void SetActiveHammerViewportId(AzFramework::ViewportId viewportId);
    AzFramework::ViewportId GetActiveHammerViewportId();

    class HammerViewportManipulatorControllerInstance final
        : public AzFramework::MultiViewportControllerInstanceInterface<HammerViewportManipulatorController>
        , public AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler
        , public AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler
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

        // AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler
        // Nothing else ever answers this bus for any Hammer viewport ID (only the real Editor
        // viewport's own EditorViewportSettings does, for its own ID - confirmed by reading
        // Code/Editor/EditorViewportWidget.cpp/.h) - so without a handler here, every one of these
        // queries silently returns its EventResult-default (false/0) for every Hammer viewport,
        // rather than erroring. That's what let entity icons render in every Hammer viewport
        // instead of just the active one: EditorHelpers::DisplayHelpers() (Code/Framework/
        // AzToolsFramework/AzToolsFramework/ViewportSelection/EditorHelpers.cpp) draws icons
        // per-viewport based on IconsVisible(viewportId's) EventResult, and true is not the
        // no-handler default. It also meant grid/angle snapping, sticky select, and manipulator
        // bound widths were silently returning false/0 for every Hammer viewport, whether or not
        // that was ever noticed - implementing this properly fixes that too. Every method here
        // except IconsVisible() (the one genuinely restricted to the active viewport) mirrors
        // EditorViewportSettings's own implementation exactly: EditorViewportWidget.cpp:2048-2111
        // confirms all twelve methods are simple passthroughs to global user preferences, not
        // anything genuinely viewport-specific - three via public AzToolsFramework:: free functions
        // (ViewportSettings.h), the rest via SandboxEditor:: free functions that are themselves
        // just AzToolsFramework::GetRegistry() reads under "/Amazon/Preferences/Editor/..." keys
        // (confirmed via Code/Editor/EditorViewportSettings.cpp) - Hammer can't call the
        // Editor-private SandboxEditor:: functions directly, but can and does read the exact same
        // Settings Registry keys with the exact same defaults.
        bool GridSnappingEnabled() const override;
        float GridSize() const override;
        bool ShowGrid() const override;
        bool AngleSnappingEnabled() const override;
        float AngleStep() const override;
        float ManipulatorLineBoundWidth() const override;
        float ManipulatorCircleBoundWidth() const override;
        bool StickySelectEnabled() const override;
        AZ::Vector3 DefaultEditorCameraPosition() const override;
        AZ::Vector2 DefaultEditorCameraOrientation() const override;
        bool IconsVisible() const override;
        bool HelpersVisible() const override;
        bool OnlyShowHelpersForSelectedEntities() const override;

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
