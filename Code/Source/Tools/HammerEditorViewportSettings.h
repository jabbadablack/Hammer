#pragma once

#include <AzFramework/Viewport/ViewportId.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace Hammer
{
    class HammerEditorViewportSettings final
        : public AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler
    {
    public:
        explicit HammerEditorViewportSettings(AzFramework::ViewportId viewport);
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
    };
}
