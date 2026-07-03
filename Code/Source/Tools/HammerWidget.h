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
    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        explicit HammerWidget(QWidget* parent);
        ~HammerWidget() override;

        AtomToolsFramework::RenderViewportWidget* GetViewportWidget() const
        {
            return m_viewportWidget;
        }

        void SetActive(bool active);
        void SetRenderTickEnabled(bool enabled);

    Q_SIGNALS:
        void ViewportFocusRequested();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void InitializeSceneIfReady();
        void InitializeScene();
        void ApplyActiveState();
        void ApplyRenderTickState();

        AtomToolsFramework::RenderViewportWidget* m_viewportWidget = nullptr;
        bool m_sceneInitialized = false;
        bool m_active = false;
        bool m_renderTickEnabled = true;
    };
}
