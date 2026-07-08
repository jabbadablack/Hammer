#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#include <Atom/RPI.Public/ViewportContext.h>
#include <AzFramework/Viewport/ViewportId.h>
#endif

class EditorViewportWidget;
class QResizeEvent;
class QShowEvent;

namespace Hammer
{
    struct HammerViewModes
    {
        bool m_normal = true;
        bool m_wireframe = false;
        bool m_overdraw = false;
    };

    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        HammerWidget(QWidget* parent, EditorViewportWidget& viewport, bool adopted);
        ~HammerWidget() override;

        EditorViewportWidget* EngineViewport() const;

        void SetRenderTickEnabled(bool enabled);

        void SetViewModes(const HammerViewModes& viewModes);
        HammerViewModes GetViewModes() const;
        void SetGameModeSuppressed(bool suppressed);

        AzFramework::ViewportId GetViewportId() const;

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;

    private:
        void ApplyRenderTickState();
        void ApplyViewModes();
        void SyncGeometry();

        EditorViewportWidget* m_engineViewport = nullptr;
        bool m_adopted = false;
        HammerViewModes m_viewModes;
        bool m_viewModesSuppressed = false;
        bool m_renderTickEnabled = false;
        AZ::RPI::ViewportContext::PipelineChangedEvent::Handler m_pipelineChangedHandler;
        AZ::RPI::ViewportContext::SizeChangedEvent::Handler m_viewportSizeChangedHandler;
    };
}
