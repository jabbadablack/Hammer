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

    class ActiveViewportTracker
    {
    public:
        void Set(AzFramework::ViewportId id)
        {
            m_id = id;
        }

        AzFramework::ViewportId Get() const
        {
            return m_id;
        }

    private:
        AzFramework::ViewportId m_id = AzFramework::InvalidViewportId;
    };

    class HammerViewportManipulatorController final
        : public AzFramework::MultiViewportController<
              HammerViewportManipulatorControllerInstance, AzFramework::ViewportControllerPriority::DispatchToAllPriorities>
    {
    public:
        explicit HammerViewportManipulatorController(AZStd::shared_ptr<ActiveViewportTracker> activeViewportTracker)
            : m_activeViewportTracker(AZStd::move(activeViewportTracker))
        {
            AZ_Assert(m_activeViewportTracker, "HammerViewportManipulatorController constructed with a null ActiveViewportTracker");
        }

        const AZStd::shared_ptr<ActiveViewportTracker>& GetActiveViewportTracker() const
        {
            return m_activeViewportTracker;
        }

    private:
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
    };

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

        void FindVisibleEntities(AZStd::vector<AZ::EntityId>& visibleEntities) override;

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
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
    };
}
