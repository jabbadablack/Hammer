#pragma once

#if !defined(Q_MOC_RUN)
#include <QScopedPointer>
#include <QWidget>
#endif

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

namespace Ui
{
    class HammerWidgetClass;
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

        static HammerWidget* CreateAdopting(QWidget* parent, QWidget& realViewport);

        void SetActive(bool active);
        void SetRenderTickEnabled(bool enabled);

    Q_SIGNALS:
        void ViewportFocusRequested();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct AdoptTag
        {
        };
        HammerWidget(QWidget* parent, QWidget& realViewport, AdoptTag);

        void InitializeSceneIfReady();
        void InitializeScene();
        void ApplyActiveState();
        void ApplyRenderTickState();
        void SyncAdoptedGeometry();

        AtomToolsFramework::RenderViewportWidget* m_viewportWidget = nullptr;
        QWidget* m_adoptedRealViewport = nullptr;
        QScopedPointer<Ui::HammerWidgetClass> m_ui;
        bool m_sceneInitialized = false;
        bool m_active = false;
        bool m_renderTickEnabled = true;
    };
}
