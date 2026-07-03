#pragma once

#include <Hammer/IHammerQtEnvironment.h>

namespace Hammer
{
    class HammerQtEnvironmentAdapter final : public IHammerQtEnvironment
    {
    public:
        HammerQtEnvironmentAdapter();
        ~HammerQtEnvironmentAdapter() override;

        void InstallMinimumSizeGuard() override;
        void RemoveMinimumSizeGuard() override;
        double DoubleClickIntervalMs() const override;
        QWidget* FindMainEditorViewport(QWidget* root) const override;
        AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget* root) const override;
        QWidget* FindViewportUiOverlayWindow(QWidget* root) const override;

    private:
        class MinimumSizeGuardFilter;
        MinimumSizeGuardFilter* m_filter = nullptr;
    };
} // namespace Hammer
