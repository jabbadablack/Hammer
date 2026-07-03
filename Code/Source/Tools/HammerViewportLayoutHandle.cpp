#include "HammerViewportLayoutHandle.h"
#include "HammerViewportLayoutWidget.h"

namespace Hammer
{
    void NullViewportLayoutHandle::SetViewportCount(int)
    {
    }

    void NullViewportLayoutHandle::ToggleMaximizeActiveViewport()
    {
    }

    LiveViewportLayoutHandle::LiveViewportLayoutHandle(HammerViewportLayoutWidget* widget)
        : m_widget(widget)
    {
        AZ_Assert(widget, "LiveViewportLayoutHandle constructed with a null widget");
        AZ_Assert(m_widget, "LiveViewportLayoutHandle failed to capture the widget it was given");
    }

    void LiveViewportLayoutHandle::SetViewportCount(int count)
    {
        m_widget && (m_widget->SetViewportCount(count), true);
    }

    void LiveViewportLayoutHandle::ToggleMaximizeActiveViewport()
    {
        m_widget && (m_widget->ToggleMaximizeActiveViewport(), true);
    }
} // namespace Hammer
