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

    bool NullViewportLayoutHandle::IsBound() const
    {
        return false;
    }

    LiveViewportLayoutHandle::LiveViewportLayoutHandle(HammerViewportLayoutWidget* widget)
        : m_widget(widget)
    {
    }

    void LiveViewportLayoutHandle::SetViewportCount(int count)
    {
        m_widget && (m_widget->SetViewportCount(count), true);
    }

    void LiveViewportLayoutHandle::ToggleMaximizeActiveViewport()
    {
        m_widget && (m_widget->ToggleMaximizeActiveViewport(), true);
    }

    bool LiveViewportLayoutHandle::IsBound() const
    {
        return m_widget != nullptr;
    }
} // namespace Hammer
