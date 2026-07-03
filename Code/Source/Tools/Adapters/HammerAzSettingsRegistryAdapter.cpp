#include "HammerAzSettingsRegistryAdapter.h"

#include <AzToolsFramework/API/SettingsRegistryUtils.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

namespace Hammer
{
    namespace
    {
        AzFramework::InputChannelId RegistryChannelId(const char* setting, const char* defaultId)
        {
            return AzFramework::InputChannelId(AzToolsFramework::GetRegistry(setting, AZStd::string(defaultId)).c_str());
        }
    } // namespace

    bool HammerAzSettingsRegistryAdapter::GridSnappingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/GridSnapping", true);
    }

    float HammerAzSettingsRegistryAdapter::GridSize() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/GridSize", 0.1));
    }

    bool HammerAzSettingsRegistryAdapter::ShowGrid() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/ShowGrid", false);
    }

    bool HammerAzSettingsRegistryAdapter::AngleSnappingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/AngleSnapping", false);
    }

    float HammerAzSettingsRegistryAdapter::AngleStep() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/AngleSize", 5.0));
    }

    float HammerAzSettingsRegistryAdapter::ManipulatorLineBoundWidth() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Manipulator/LineBoundWidth", 0.1));
    }

    float HammerAzSettingsRegistryAdapter::ManipulatorCircleBoundWidth() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Manipulator/CircleBoundWidth", 0.1));
    }

    bool HammerAzSettingsRegistryAdapter::StickySelectEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/StickySelect", false);
    }

    AZ::Vector3 HammerAzSettingsRegistryAdapter::DefaultEditorCameraPosition() const
    {
        return AZ::Vector3(
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/x", 0.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/y", -10.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/z", 4.0)));
    }

    AZ::Vector2 HammerAzSettingsRegistryAdapter::DefaultEditorCameraOrientation() const
    {
        return AZ::Vector2(
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPitch", 0.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingYaw", 0.0)));
    }

    bool HammerAzSettingsRegistryAdapter::HelpersVisible() const
    {
        return AzToolsFramework::HelpersVisible();
    }

    bool HammerAzSettingsRegistryAdapter::OnlyShowHelpersForSelectedEntities() const
    {
        return AzToolsFramework::OnlyShowHelpersForSelectedEntities();
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraTranslateForwardChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateForwardId", "keyboard_key_alphanumeric_W");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraTranslateBackwardChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateBackwardId", "keyboard_key_alphanumeric_S");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraTranslateLeftChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateLeftId", "keyboard_key_alphanumeric_A");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraTranslateRightChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateRightId", "keyboard_key_alphanumeric_D");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraTranslateUpChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateUpId", "keyboard_key_alphanumeric_E");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraTranslateDownChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/CameraTranslateUpDownId", "keyboard_key_alphanumeric_Q");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraTranslateBoostChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/TranslateBoostId", "keyboard_key_modifier_shift_l");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraFreeLookChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/FreeLookId", "mouse_button_right");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraOrbitChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/OrbitId", "keyboard_key_modifier_alt_l");
    }

    AzFramework::InputChannelId HammerAzSettingsRegistryAdapter::CameraOrbitLookChannelId() const
    {
        return RegistryChannelId("/Amazon/Preferences/Editor/Camera/OrbitLookId", "mouse_button_left");
    }

    float HammerAzSettingsRegistryAdapter::CameraRotateSpeed() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSpeed", 0.005));
    }

    bool HammerAzSettingsRegistryAdapter::CameraCaptureCursorForLook() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/CaptureCursorLook", true);
    }

    float HammerAzSettingsRegistryAdapter::CameraTranslateSpeed() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSpeed", 10.0));
    }

    float HammerAzSettingsRegistryAdapter::CameraBoostMultiplier() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/BoostMultiplier", 3.0));
    }

    float HammerAzSettingsRegistryAdapter::CameraDollyScrollSpeed() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DollyScrollSpeed", 0.02));
    }

    float HammerAzSettingsRegistryAdapter::CameraDefaultOrbitDistance() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultOrbitDistance", 20.0));
    }

    float HammerAzSettingsRegistryAdapter::CameraRotateSmoothness() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSmoothness", 5.0));
    }

    float HammerAzSettingsRegistryAdapter::CameraTranslateSmoothness() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSmoothness", 5.0));
    }

    bool HammerAzSettingsRegistryAdapter::CameraRotateSmoothingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/RotateSmoothing", true);
    }

    bool HammerAzSettingsRegistryAdapter::CameraTranslateSmoothingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/TranslateSmoothing", true);
    }
} // namespace Hammer
