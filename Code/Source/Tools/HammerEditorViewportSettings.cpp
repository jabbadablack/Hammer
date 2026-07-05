#include "HammerEditorViewportSettings.h"

#include <Hammer/HammerEditorViewportBus.h>

#include <AzToolsFramework/Viewport/ViewportSettings.h>
#include <Editor/EditorViewportSettings.h>

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
        return SandboxEditor::GridSnappingEnabled();
    }

    float HammerEditorViewportSettings::GridSize() const
    {
        return SandboxEditor::GridSnappingSize();
    }

    bool HammerEditorViewportSettings::ShowGrid() const
    {
        return SandboxEditor::ShowingGrid();
    }

    bool HammerEditorViewportSettings::AngleSnappingEnabled() const
    {
        return SandboxEditor::AngleSnappingEnabled();
    }

    float HammerEditorViewportSettings::AngleStep() const
    {
        return SandboxEditor::AngleSnappingSize();
    }

    float HammerEditorViewportSettings::ManipulatorLineBoundWidth() const
    {
        return SandboxEditor::ManipulatorLineBoundWidth();
    }

    float HammerEditorViewportSettings::ManipulatorCircleBoundWidth() const
    {
        return SandboxEditor::ManipulatorCircleBoundWidth();
    }

    bool HammerEditorViewportSettings::StickySelectEnabled() const
    {
        return SandboxEditor::StickySelectEnabled();
    }

    AZ::Vector3 HammerEditorViewportSettings::DefaultEditorCameraPosition() const
    {
        return SandboxEditor::CameraDefaultEditorPosition();
    }

    AZ::Vector2 HammerEditorViewportSettings::DefaultEditorCameraOrientation() const
    {
        return SandboxEditor::CameraDefaultEditorOrientation();
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
