
#pragma once

#include <Atom/RPI.Public/Base.h>

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
    // A Hammer-owned viewport, entirely built from public AzFramework/AzToolsFramework/
    // AtomToolsFramework APIs (no Editor-private types).
    // isPrimary == true: gets its own camera controller (HammerViewportCameraFactory), matching
    // the default Editor viewport's feel, and (being the first RenderViewportWidget constructed)
    // naturally becomes the default AZ::RPI::ViewportContext.
    // isPrimary == false: has no camera input of its own; instead it mirrors the default
    // viewport's camera (AZ::RPI::ViewportContextRequestsInterface::GetDefaultViewportContext())
    // every time it changes, since there should be no independent input here.
    // Selection and gizmos on both come from HammerViewportManipulatorController, which uses only
    // public AzToolsFramework APIs.
    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        explicit HammerWidget(bool isPrimary, QWidget* parent = nullptr);
        ~HammerWidget() override = default;

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
        bool m_isPrimary = false;
        bool m_sceneInitialized = false;
    };
}
