#include "HammerViewportDisplayController.h"

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzToolsFramework/Viewport/ViewportSettings.h>
#include <Editor/EditorViewportSettings.h>
#include <Hammer/HammerEditorViewportBus.h>

namespace Hammer
{
    namespace ViewInt = AzToolsFramework::ViewportInteraction;

    HammerViewportDisplayControllerInstance::HammerViewportDisplayControllerInstance(
        AzFramework::ViewportId viewport, HammerViewportDisplayController* controller)
        : AzFramework::MultiViewportControllerInstanceInterface<HammerViewportDisplayController>(viewport, controller)
    {
        AZ_Assert(controller, "HammerViewportDisplayControllerInstance constructed with a null controller");
        AZ_Assert(
            viewport != AzFramework::InvalidViewportId,
            "HammerViewportDisplayControllerInstance constructed with an invalid ViewportId");
        ViewInt::EditorEntityViewportInteractionRequestBus::Handler::BusConnect(viewport);
        ViewInt::ViewportSettingsRequestBus::Handler::BusConnect(viewport);
    }

    HammerViewportDisplayControllerInstance::~HammerViewportDisplayControllerInstance()
    {
        ViewInt::ViewportSettingsRequestBus::Handler::BusDisconnect();
        ViewInt::EditorEntityViewportInteractionRequestBus::Handler::BusDisconnect();
    }

    void HammerViewportDisplayControllerInstance::FindVisibleEntities(AZStd::vector<AZ::EntityId>& visibleEntities)
    {
        visibleEntities.assign(m_entityVisibilityQuery.Begin(), m_entityVisibilityQuery.End());
    }

    void HammerViewportDisplayControllerInstance::UpdateViewport(const AzFramework::ViewportControllerUpdateEvent& event)
    {
        AZ_Assert(
            event.m_viewportId == GetViewportId(),
            "HammerViewportDisplayControllerInstance received an update for viewport %d instead of its own %d",
            event.m_viewportId, GetViewportId());

        (event.m_priority == AzFramework::ViewportControllerPriority::Normal) && (RefreshDisplay(), true);
    }

    void HammerViewportDisplayControllerInstance::RefreshDisplay()
    {
        AzFramework::CameraState cameraState;
        ViewInt::ViewportInteractionRequestBus::EventResult(
            cameraState, GetViewportId(), &ViewInt::ViewportInteractionRequestBus::Events::GetCameraState);
        m_entityVisibilityQuery.UpdateVisibility(cameraState);

        AzFramework::DebugDisplayRequestBus::BusPtr debugDisplayBus;
        AzFramework::DebugDisplayRequestBus::Bind(debugDisplayBus, GetViewportId());
        AzFramework::DebugDisplayRequests* debugDisplay = AzFramework::DebugDisplayRequestBus::FindFirstHandler(debugDisplayBus);
        debugDisplay &&
            (AzFramework::ViewportDebugDisplayEventBus::Event(
                 AzToolsFramework::GetEntityContextId(), &AzFramework::ViewportDebugDisplayEvents::DisplayViewport,
                 AzFramework::ViewportInfo{ GetViewportId() }, *debugDisplay),
             true);
    }

    bool HammerViewportDisplayControllerInstance::GridSnappingEnabled() const
    {
        return SandboxEditor::GridSnappingEnabled();
    }

    float HammerViewportDisplayControllerInstance::GridSize() const
    {
        return SandboxEditor::GridSnappingSize();
    }

    bool HammerViewportDisplayControllerInstance::ShowGrid() const
    {
        return SandboxEditor::ShowingGrid();
    }

    bool HammerViewportDisplayControllerInstance::AngleSnappingEnabled() const
    {
        return SandboxEditor::AngleSnappingEnabled();
    }

    float HammerViewportDisplayControllerInstance::AngleStep() const
    {
        return SandboxEditor::AngleSnappingSize();
    }

    float HammerViewportDisplayControllerInstance::ManipulatorLineBoundWidth() const
    {
        return SandboxEditor::ManipulatorLineBoundWidth();
    }

    float HammerViewportDisplayControllerInstance::ManipulatorCircleBoundWidth() const
    {
        return SandboxEditor::ManipulatorCircleBoundWidth();
    }

    bool HammerViewportDisplayControllerInstance::StickySelectEnabled() const
    {
        return SandboxEditor::StickySelectEnabled();
    }

    AZ::Vector3 HammerViewportDisplayControllerInstance::DefaultEditorCameraPosition() const
    {
        return SandboxEditor::CameraDefaultEditorPosition();
    }

    AZ::Vector2 HammerViewportDisplayControllerInstance::DefaultEditorCameraOrientation() const
    {
        return SandboxEditor::CameraDefaultEditorOrientation();
    }

    bool HammerViewportDisplayControllerInstance::IconsVisible() const
    {
        AzFramework::ViewportId activeViewportId = AzFramework::InvalidViewportId;
        HammerEditorActiveViewportRequestBus::BroadcastResult(activeViewportId, &HammerEditorActiveViewportRequests::GetActiveViewportId);
        return GetViewportId() == activeViewportId;
    }

    bool HammerViewportDisplayControllerInstance::HelpersVisible() const
    {
        return AzToolsFramework::HelpersVisible();
    }

    bool HammerViewportDisplayControllerInstance::OnlyShowHelpersForSelectedEntities() const
    {
        return AzToolsFramework::OnlyShowHelpersForSelectedEntities();
    }
}
