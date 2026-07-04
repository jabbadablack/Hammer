#pragma once

#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

namespace Hammer
{
    class HammerRenderViewportWidget : public AtomToolsFramework::RenderViewportWidget
    {
    public:
        using RenderViewportWidget::RenderViewportWidget;

        AzFramework::WindowSize GetClientAreaSize() const override;
        AzFramework::WindowSize GetRenderResolution() const override;
    };
} // namespace Hammer
