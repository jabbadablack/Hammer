
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/smart_ptr/shared_ptr.h>

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
    class ActiveViewportTracker;

    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        HammerWidget(QWidget* parent, AZStd::shared_ptr<ActiveViewportTracker> activeViewportTracker);
        ~HammerWidget() override = default;

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
        void ApplyActiveState();
        void ApplyRenderTickState();

        AtomToolsFramework::RenderViewportWidget* m_viewportWidget = nullptr;
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
        bool m_sceneInitialized = false;
        bool m_active = false;
        bool m_renderTickEnabled = true;
    };
}
