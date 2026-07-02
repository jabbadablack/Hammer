#pragma once

#include "HammerLazyFind.h"

#if !defined(Q_MOC_RUN)
#include <QObject>
#endif

#include <Atom/RPI.Public/Base.h>

class QEvent;
class QTimer;
class QWidget;

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

namespace Hammer
{
    class HammerWidget;

    class HammerHiddenViewportProxy
        : public QObject
    {
        Q_OBJECT
    public:
        explicit HammerHiddenViewportProxy(QWidget* gridContainer, QObject* parent = nullptr);

        void SetHiddenRealViewport(QWidget* realViewport);
        void SetActiveViewport(HammerWidget* activeViewport);

    private:
        void SyncToActive();
        bool eventFilter(QObject* watched, QEvent* event) override;

        QWidget* m_gridContainer = nullptr;
        QWidget* m_hiddenRealViewport = nullptr;
        HammerWidget* m_activeViewport = nullptr;
        QTimer* m_syncTimer = nullptr;
        AZ::RPI::ViewportContextPtr m_hiddenRealViewportContext;
        LazyFind<QWidget> m_viewportUiOverlayWindow;
        LazyFind<AtomToolsFramework::RenderViewportWidget> m_realInternalRenderViewport;
    };
}
