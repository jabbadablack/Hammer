
#pragma once

#include <Atom/RPI.Public/Base.h>
#include <AzFramework/Windowing/WindowBus.h>

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

class QResizeEvent;
class QShowEvent;

namespace Hammer
{
    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        // wireframe enables Hammer's wireframe overlay on top of the normal default rendering.
        // cameraSource: when non-null, this viewport has no camera controller of its own and
        // instead mirrors cameraSource's camera transform every time it changes (used for
        // viewports that follow another viewport's camera instead of navigating independently).
        explicit HammerWidget(
            bool wireframe = true, AtomToolsFramework::RenderViewportWidget* cameraSource = nullptr, QWidget* parent = nullptr);
        ~HammerWidget() override;

        AtomToolsFramework::RenderViewportWidget* GetViewportWidget() const
        {
            return m_viewportWidget;
        }

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:
        // Binds the scene/render pipeline (and camera/manipulator controllers) to
        // m_viewportWidget. Deferred until the viewport widget first reports a valid, non-zero
        // size instead of running unconditionally in the constructor - doing it too early (e.g.
        // while freshly created as dock-pane content still mid-layout) let the Atom render
        // pipeline get built against degenerate 0-size geometry, which crashed the Editor
        // (LightCullingPass/ReflectionScreenSpaceTracePass building a zero-sized buffer).
        void InitializeSceneIfReady();

        AtomToolsFramework::RenderViewportWidget* m_viewportWidget = nullptr;
        AZ::RPI::MatrixChangedEvent::Handler m_cameraTransformChangedHandler;
        AzFramework::NativeWindowHandle m_wireframeWindowHandle = nullptr;
        AtomToolsFramework::RenderViewportWidget* m_cameraSource = nullptr;
        bool m_wireframe = false;
        bool m_sceneInitialized = false;
    };
}
