
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

class QResizeEvent;
class QShowEvent;
class QEvent;
class QObject;

namespace Hammer
{
    // A Hammer-owned viewport, entirely built from public AzFramework/AzToolsFramework/
    // AtomToolsFramework APIs (no Editor-private types). Each instance gets its own independent
    // camera controller (HammerViewportCameraFactory), matching the default Editor viewport's
    // feel. Selection and gizmos come from HammerViewportManipulatorController, which uses only
    // public AzToolsFramework APIs.
    //
    // Only one HammerWidget across a whole grid is ever "active" (input-processing enabled) at a
    // time - see SetActive(). This works around a genuine engine limitation: several
    // AzToolsFramework systems reached via the DisplayViewport broadcast (the entity
    // visibility/pick cache used for click-to-select, and the entity-icon-billboard renderer) use
    // single, unkeyed state shared across every viewport, built on a single-active-viewport
    // assumption that's true in the real Editor but breaks when multiple Hammer viewports are
    // simultaneously live. Disabling input processing on inactive viewports also stops their
    // periodic tick (AzFramework::ViewportControllerList::UpdateViewport() no-ops while disabled),
    // which is what stops them contributing to those shared caches at all.
    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        explicit HammerWidget(QWidget* parent = nullptr);
        ~HammerWidget() override = default;

        AtomToolsFramework::RenderViewportWidget* GetViewportWidget() const
        {
            return m_viewportWidget;
        }

        // Enables or disables input processing (camera control, clicks, and the per-tick
        // DisplayViewport broadcast) on this viewport's RenderViewportWidget. Safe to call before
        // the scene has finished initializing - the requested state is applied once
        // InitializeSceneIfReady() actually builds the controller list.
        void SetActive(bool active);

    Q_SIGNALS:
        // Emitted when this viewport's RenderViewportWidget gains Qt focus (see eventFilter()).
        void ViewportFocusRequested();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        // Binds the scene/render pipeline (and camera/manipulator controllers) to
        // m_viewportWidget. Deferred until the viewport widget first reports a valid, non-zero
        // size instead of running unconditionally in the constructor - doing it too early (e.g.
        // while freshly created as dock-pane content still mid-layout) let the Atom render
        // pipeline get built against degenerate 0-size geometry, which crashed the Editor
        // (LightCullingPass/ReflectionScreenSpaceTracePass building a zero-sized buffer).
        void InitializeSceneIfReady();

        AtomToolsFramework::RenderViewportWidget* m_viewportWidget = nullptr;
        bool m_sceneInitialized = false;
        bool m_active = false;
    };
}
