#pragma once

#include <Hammer/IHammerQtEnvironment.h>

namespace Hammer
{
    class HammerNullQtEnvironment final : public IHammerQtEnvironment
    {
    public:
        void InstallMinimumSizeGuard() override {}
        void RemoveMinimumSizeGuard() override {}
        double DoubleClickIntervalMs() const override { return 400.0; }
        QWidget* FindMainEditorViewport(QWidget*) const override { return nullptr; }
        AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget*) const override { return nullptr; }
        QWidget* FindViewportUiOverlayWindow(QWidget*) const override { return nullptr; }
    };
} // namespace Hammer
