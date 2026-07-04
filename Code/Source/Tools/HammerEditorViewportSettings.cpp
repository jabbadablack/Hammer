#include "HammerEditorViewportSettings.h"

#include <Hammer/HammerEditorViewportBus.h>

#include <AzToolsFramework/API/SettingsRegistryUtils.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

namespace Hammer
{
    HammerEditorViewportSettings::HammerEditorViewportSettings(AzFramework::ViewportId viewport)
        : m_viewportId(viewport)
    {
        AZ_Assert(viewport != AzFramework::InvalidViewportId, "HammerEditorViewportSettings constructed with an invalid ViewportId");
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusConnect(viewport);
        AZ_Assert(
            AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusIsConnectedId(viewport),
            "HammerEditorViewportSettings failed to connect to ViewportSettingsRequestBus for viewport %u",
            static_cast<unsigned>(viewport));
    }

    HammerEditorViewportSettings::~HammerEditorViewportSettings()
    {
        AZ_Assert(
            AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusIsConnected(),
            "HammerEditorViewportSettings was not connected to ViewportSettingsRequestBus at destruction");
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusDisconnect();
    }

    bool HammerEditorViewportSettings::GridSnappingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/GridSnapping", true);
    }

    float HammerEditorViewportSettings::GridSize() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/GridSize", 0.1));
    }

    bool HammerEditorViewportSettings::ShowGrid() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/ShowGrid", false);
    }

    bool HammerEditorViewportSettings::AngleSnappingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/AngleSnapping", false);
    }

    float HammerEditorViewportSettings::AngleStep() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/AngleSize", 5.0));
    }

    float HammerEditorViewportSettings::ManipulatorLineBoundWidth() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Manipulator/LineBoundWidth", 0.1));
    }

    float HammerEditorViewportSettings::ManipulatorCircleBoundWidth() const
    {
        return aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Manipulator/CircleBoundWidth", 0.1));
    }

    bool HammerEditorViewportSettings::StickySelectEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/StickySelect", false);
    }

    AZ::Vector3 HammerEditorViewportSettings::DefaultEditorCameraPosition() const
    {
        return AZ::Vector3(
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/x", 0.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/y", -10.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPosition/z", 4.0)));
    }

    AZ::Vector2 HammerEditorViewportSettings::DefaultEditorCameraOrientation() const
    {
        return AZ::Vector2(
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingPitch", 0.0)),
            aznumeric_cast<float>(AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/Camera/DefaultStartingYaw", 0.0)));
    }

    bool HammerEditorViewportSettings::IconsVisible() const
    {
        AzFramework::ViewportId activeViewportId = AzFramework::InvalidViewportId;
        HammerEditorActiveViewportRequestBus::BroadcastResult(activeViewportId, &HammerEditorActiveViewportRequests::GetActiveViewportId);
        return m_viewportId == activeViewportId;
    }

    bool HammerEditorViewportSettings::HelpersVisible() const
    {
        return AzToolsFramework::HelpersVisible();
    }

    bool HammerEditorViewportSettings::OnlyShowHelpersForSelectedEntities() const
    {
        return AzToolsFramework::OnlyShowHelpersForSelectedEntities();
    }
}
