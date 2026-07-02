#include "HammerActiveViewportTracker.h"

namespace Hammer
{
    ActiveViewportTracker::ActiveViewportTracker()
    {
        HammerEditorActiveViewportRequestBus::Handler::BusConnect();
    }

    ActiveViewportTracker::~ActiveViewportTracker()
    {
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
