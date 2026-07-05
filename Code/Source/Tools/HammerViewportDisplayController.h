#pragma once

#include <AzFramework/Viewport/MultiViewportController.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzFramework/Visibility/EntityVisibilityQuery.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace Hammer
{
    class HammerViewportDisplayControllerInstance;

    class HammerViewportDisplayController final
        : public AzFramework::MultiViewportController<
              HammerViewportDisplayControllerInstance, AzFramework::ViewportControllerPriority::DispatchToAllPriorities>
    {
    };

    class HammerViewportDisplayControllerInstance final
        : public AzFramework::MultiViewportControllerInstanceInterface<HammerViewportDisplayController>
        , public AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler
        , public AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler
    {
    public:
        HammerViewportDisplayControllerInstance(AzFramework::ViewportId viewport, HammerViewportDisplayController* controller);
        ~HammerViewportDisplayControllerInstance() override;

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
        void RefreshDisplay();

        AzFramework::EntityVisibilityQuery m_entityVisibilityQuery;
    };
}
