#include "HammerViewportDisplayController.h"
#include "HammerEditorViewportSettings.h"

#include <AzFramework/Entity/EntityDebugDisplayBus.h>

namespace Hammer
{
    HammerViewportDisplayControllerInstance::HammerViewportDisplayControllerInstance(
        AzFramework::ViewportId viewport, HammerViewportDisplayController* controller)
        : AzFramework::MultiViewportControllerInstanceInterface<HammerViewportDisplayController>(viewport, controller)
        , m_viewportSettings(AZStd::make_unique<HammerEditorViewportSettings>(viewport))
    {
        AZ_Assert(controller, "HammerViewportDisplayControllerInstance constructed with a null controller");
        AZ_Assert(
            viewport != AzFramework::InvalidViewportId,
            "HammerViewportDisplayControllerInstance constructed with an invalid ViewportId");
        AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler::BusConnect(viewport);
    }

    HammerViewportDisplayControllerInstance::~HammerViewportDisplayControllerInstance()
    {
        AzToolsFramework::ViewportInteraction::EditorEntityViewportInteractionRequestBus::Handler::BusDisconnect();
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
        AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::EventResult(
            cameraState, GetViewportId(), &AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::Events::GetCameraState);
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
}
