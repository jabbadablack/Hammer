#pragma once

#include <Hammer/IHammerSettingsProvider.h>

namespace Hammer
{
    class HammerAzSettingsRegistryAdapter final : public IHammerSettingsProvider
    {
    public:
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
        bool HelpersVisible() const override;
        bool OnlyShowHelpersForSelectedEntities() const override;

        AzFramework::InputChannelId CameraTranslateForwardChannelId() const override;
        AzFramework::InputChannelId CameraTranslateBackwardChannelId() const override;
        AzFramework::InputChannelId CameraTranslateLeftChannelId() const override;
        AzFramework::InputChannelId CameraTranslateRightChannelId() const override;
        AzFramework::InputChannelId CameraTranslateUpChannelId() const override;
        AzFramework::InputChannelId CameraTranslateDownChannelId() const override;
        AzFramework::InputChannelId CameraTranslateBoostChannelId() const override;
        AzFramework::InputChannelId CameraFreeLookChannelId() const override;
        AzFramework::InputChannelId CameraOrbitChannelId() const override;
        AzFramework::InputChannelId CameraOrbitLookChannelId() const override;
        float CameraRotateSpeed() const override;
        bool CameraCaptureCursorForLook() const override;
        float CameraTranslateSpeed() const override;
        float CameraBoostMultiplier() const override;
        float CameraDollyScrollSpeed() const override;
        float CameraDefaultOrbitDistance() const override;
        float CameraRotateSmoothness() const override;
        float CameraTranslateSmoothness() const override;
        bool CameraRotateSmoothingEnabled() const override;
        bool CameraTranslateSmoothingEnabled() const override;
    };
} // namespace Hammer
