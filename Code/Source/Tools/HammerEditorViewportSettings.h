#pragma once

#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace Hammer
{
    class ActiveViewportTracker;

    class HammerEditorViewportSettings final
        : public AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler
    {
    public:
        HammerEditorViewportSettings(AzFramework::ViewportId viewport, AZStd::shared_ptr<ActiveViewportTracker> activeViewportTracker);
        ~HammerEditorViewportSettings() override;

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
        AzFramework::ViewportId m_viewportId;
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
    };
}
