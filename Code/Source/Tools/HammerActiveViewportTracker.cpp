#include "HammerActiveViewportTracker.h"

namespace Hammer
{
    ActiveViewportTracker::ActiveViewportTracker()
    {
        AZ_Assert(
            !HammerEditorActiveViewportRequestBus::Handler::BusIsConnected(),
            "ActiveViewportTracker constructed while already connected to HammerEditorActiveViewportRequestBus");
        HammerEditorActiveViewportRequestBus::Handler::BusConnect();
        AZ_Assert(
            HammerEditorActiveViewportRequestBus::Handler::BusIsConnected(),
            "ActiveViewportTracker failed to connect to HammerEditorActiveViewportRequestBus");
    }

    ActiveViewportTracker::~ActiveViewportTracker()
    {
        AZ_Assert(
            HammerEditorActiveViewportRequestBus::Handler::BusIsConnected(),
            "ActiveViewportTracker was not connected to HammerEditorActiveViewportRequestBus at destruction");
        HammerEditorActiveViewportRequestBus::Handler::BusDisconnect();
    }

    void ActiveViewportTracker::SetActiveViewportId(AzFramework::ViewportId viewportId)
    {
        m_id = viewportId;
    }

    AzFramework::ViewportId ActiveViewportTracker::GetActiveViewportId() const
    {
        return m_id;
    }
}
