#include "HammerHiddenViewportProxy.h"
#include "HammerWidget.h"

#include "HammerNames.h"

#include <Hammer/IHammerQtEnvironment.h>
#include <Hammer/IHammerRenderBackend.h>

#include <AzCore/Interface/Interface.h>

#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

#include <QEvent>
#include <QTimer>
#include <QWidget>

namespace Hammer
{
    HammerHiddenViewportProxy::HammerHiddenViewportProxy(QWidget& gridContainer, QObject* parent)
        : QObject(parent)
        , m_gridContainer(gridContainer)
    {
        m_syncTimer = new QTimer(this);
        connect(m_syncTimer, &QTimer::timeout, this, &HammerHiddenViewportProxy::SyncToActive);
        m_syncTimer->start(16);
    }

    void HammerHiddenViewportProxy::SetHiddenRealViewport(QWidget& realViewport)
    {
        realViewport.setParent(&m_gridContainer);
        realViewport.setAttribute(Qt::WA_TransparentForMouseEvents, true);
        realViewport.setFocusPolicy(Qt::NoFocus);
        realViewport.show();
        m_hiddenRealViewport = &realViewport;

        auto* renderBackend = AZ::Interface<IHammerRenderBackend>::Get();
        AZ_Assert(renderBackend, "IHammerRenderBackend must be registered before SetHiddenRealViewport is called");
        m_hiddenRealViewportContext = renderBackend->FindViewportContextByName(Names::HiddenPerspectiveViewportContextAzName());
        AZ_Error(
            "HammerHiddenViewportProxy", m_hiddenRealViewportContext,
            "Could not resolve the real viewport's own ViewportContext by name - camera syncing will not work");
    }

    void HammerHiddenViewportProxy::SetActiveViewport(HammerWidget& activeViewport)
    {
        m_activeViewport = &activeViewport;
    }

    void HammerHiddenViewportProxy::SyncToActive()
    {
        const bool ready = m_hiddenRealViewport && m_activeViewport;

        ready && (m_hiddenRealViewport->setGeometry(-10000, -10000, m_activeViewport->width(), m_activeViewport->height()), true);

        AtomToolsFramework::RenderViewportWidget* activeViewportWidget = nullptr;
        ready && (activeViewportWidget = m_activeViewport->GetViewportWidget(), true);

        AZ::RPI::ViewportContextPtr activeContext;
        activeViewportWidget && (activeContext = activeViewportWidget->GetViewportContext(), true);

        auto* renderBackend = AZ::Interface<IHammerRenderBackend>::Get();
        AZ_Assert(renderBackend, "IHammerRenderBackend must be registered before SyncToActive is called");
        ready && (renderBackend->SyncActiveCamera(m_hiddenRealViewportContext, activeContext), true);

        auto* qtEnvironment = AZ::Interface<IHammerQtEnvironment>::Get();
        AZ_Assert(qtEnvironment, "IHammerQtEnvironment must be registered before SyncToActive is called");

        ready &&
            (m_viewportUiOverlayWindow.Get(
                 [this, qtEnvironment]() -> QWidget*
                 {
                     QWidget* found = qtEnvironment->FindViewportUiOverlayWindow(m_hiddenRealViewport);
                     found && (found->installEventFilter(this), true);
                     return found;
                 }),
             true);

        ready &&
            (m_realInternalRenderViewport.Get(
                 [this, qtEnvironment]() -> AtomToolsFramework::RenderViewportWidget*
                 {
                     return qtEnvironment->FindRealRenderViewport(m_hiddenRealViewport);
                 }),
             true);
    }

    bool HammerHiddenViewportProxy::eventFilter(QObject* watched, QEvent* event)
    {
        const bool isOverlayMoveOrResize = watched == m_viewportUiOverlayWindow.Peek() && m_activeViewport &&
            (event->type() == QEvent::Move || event->type() == QEvent::Resize);

        QPoint desiredTopLeft;
        isOverlayMoveOrResize && (desiredTopLeft = m_activeViewport->mapToGlobal(QPoint(0, 0)), true);

        (isOverlayMoveOrResize && m_viewportUiOverlayWindow.Peek()->pos() != desiredTopLeft) &&
            (m_viewportUiOverlayWindow.Peek()->move(desiredTopLeft), true);

        return QObject::eventFilter(watched, event);
    }
}
