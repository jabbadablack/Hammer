#pragma once

#include <Hammer/IHammerSettingsProvider.h>

namespace Hammer
{
    class HammerNullSettingsProvider final : public IHammerSettingsProvider
    {
    public:
        bool GridSnappingEnabled() const override { return true; }
        float GridSize() const override { return 0.1f; }
        bool ShowGrid() const override { return false; }
        bool AngleSnappingEnabled() const override { return false; }
        float AngleStep() const override { return 5.0f; }
        float ManipulatorLineBoundWidth() const override { return 0.1f; }
        float ManipulatorCircleBoundWidth() const override { return 0.1f; }
        bool StickySelectEnabled() const override { return false; }
        AZ::Vector3 DefaultEditorCameraPosition() const override { return AZ::Vector3(0.0f, -10.0f, 4.0f); }
        AZ::Vector2 DefaultEditorCameraOrientation() const override { return AZ::Vector2(0.0f, 0.0f); }
        bool HelpersVisible() const override { return false; }
        bool OnlyShowHelpersForSelectedEntities() const override { return false; }

        AzFramework::InputChannelId CameraTranslateForwardChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_alphanumeric_W");
        }
        AzFramework::InputChannelId CameraTranslateBackwardChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_alphanumeric_S");
        }
        AzFramework::InputChannelId CameraTranslateLeftChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_alphanumeric_A");
        }
        AzFramework::InputChannelId CameraTranslateRightChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_alphanumeric_D");
        }
        AzFramework::InputChannelId CameraTranslateUpChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_alphanumeric_E");
        }
        AzFramework::InputChannelId CameraTranslateDownChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_alphanumeric_Q");
        }
        AzFramework::InputChannelId CameraTranslateBoostChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_modifier_shift_l");
        }
        AzFramework::InputChannelId CameraFreeLookChannelId() const override
        {
            return AzFramework::InputChannelId("mouse_button_right");
        }
        AzFramework::InputChannelId CameraOrbitChannelId() const override
        {
            return AzFramework::InputChannelId("keyboard_key_modifier_alt_l");
        }
        AzFramework::InputChannelId CameraOrbitLookChannelId() const override
        {
            return AzFramework::InputChannelId("mouse_button_left");
        }
        float CameraRotateSpeed() const override { return 0.005f; }
        bool CameraCaptureCursorForLook() const override { return true; }
        float CameraTranslateSpeed() const override { return 10.0f; }
        float CameraBoostMultiplier() const override { return 3.0f; }
        float CameraDollyScrollSpeed() const override { return 0.02f; }
        float CameraDefaultOrbitDistance() const override { return 20.0f; }
        float CameraRotateSmoothness() const override { return 5.0f; }
        float CameraTranslateSmoothness() const override { return 5.0f; }
        bool CameraRotateSmoothingEnabled() const override { return true; }
        bool CameraTranslateSmoothingEnabled() const override { return true; }
    };
} // namespace Hammer
