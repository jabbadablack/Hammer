#pragma once

class QWidget;

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

namespace Hammer
{
    void InstallMinimumSizeGuard();
    void RemoveMinimumSizeGuard();
    double DoubleClickIntervalMs();
    QWidget* FindMainEditorViewport(QWidget* root);
    AtomToolsFramework::RenderViewportWidget* FindRealRenderViewport(QWidget* root);
    QWidget* FindViewportUiOverlayWindow(QWidget* root);
} // namespace Hammer
