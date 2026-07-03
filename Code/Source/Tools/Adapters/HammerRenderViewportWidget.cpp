#include "HammerRenderViewportWidget.h"

#include <AzCore/std/algorithm.h>

namespace Hammer
{
    AzFramework::WindowSize HammerRenderViewportWidget::GetClientAreaSize() const
    {
        AzFramework::WindowSize size = RenderViewportWidget::GetClientAreaSize();
        size.m_width = AZStd::max(size.m_width, 32u);
        size.m_height = AZStd::max(size.m_height, 32u);
        return size;
    }

    AzFramework::WindowSize HammerRenderViewportWidget::GetRenderResolution() const
    {
        AzFramework::WindowSize size = RenderViewportWidget::GetRenderResolution();
        size.m_width = AZStd::max(size.m_width, 32u);
        size.m_height = AZStd::max(size.m_height, 32u);
        return size;
    }
} // namespace Hammer
