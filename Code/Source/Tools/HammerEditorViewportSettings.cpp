#include "HammerEditorViewportSettings.h"
#include "HammerActiveViewportTracker.h"

#include <AzToolsFramework/API/SettingsRegistryUtils.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>

namespace Hammer
{
    HammerEditorViewportSettings::HammerEditorViewportSettings(
        AzFramework::ViewportId viewport, AZStd::shared_ptr<ActiveViewportTracker> activeViewportTracker)
        : m_viewportId(viewport)
        , m_activeViewportTracker(AZStd::move(activeViewportTracker))
    {
        AZ_Assert(m_activeViewportTracker, "HammerEditorViewportSettings constructed with a null ActiveViewportTracker");
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusConnect(viewport);
    }

    HammerEditorViewportSettings::~HammerEditorViewportSettings()
    {
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Handler::BusDisconnect();
    }

    bool HammerEditorViewportSettings::GridSnappingEnabled() const
    {
        return AzToolsFramework::GetRegistry("/Amazon/Preferences/Editor/GridSnapping", false);
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
        return m_viewportId == m_activeViewportTracker->GetActiveViewportId();
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
