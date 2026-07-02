#include "HammerHiddenViewportProxy.h"
#include "HammerQtWidgetUtils.h"
#include "HammerWidget.h"

#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

#include <QEvent>
#include <QTimer>
#include <QWidget>

namespace Hammer
{
    HammerHiddenViewportProxy::HammerHiddenViewportProxy(QWidget* gridContainer, QObject* parent)
        : QObject(parent)
        , m_gridContainer(gridContainer)
    {
        AZ_Assert(m_gridContainer, "HammerHiddenViewportProxy constructed with a null grid container");

        m_syncTimer = new QTimer(this);
        connect(m_syncTimer, &QTimer::timeout, this, &HammerHiddenViewportProxy::SyncToActive);
        m_syncTimer->start(16);
    }

    void HammerHiddenViewportProxy::SetHiddenRealViewport(QWidget* realViewport)
    {
        AZ_Assert(realViewport, "SetHiddenRealViewport called with a null widget");
        if (!realViewport)
        {
            return;
        }

        AZ_Assert(m_gridContainer, "m_gridContainer must be constructed before hosting the real viewport");
        realViewport->setParent(m_gridContainer);

        realViewport->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        realViewport->setFocusPolicy(Qt::NoFocus);
        realViewport->show();
        m_hiddenRealViewport = realViewport;

        if (auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get())
        {
            m_hiddenRealViewportContext = viewportContextManager->GetViewportContextByName(AZ::Name("Hammer Hidden Perspective Viewport"));
            AZ_Error(
                "HammerHiddenViewportProxy", m_hiddenRealViewportContext,
                "Could not resolve the real viewport's own ViewportContext by name - camera syncing will not work");
        }
    }

    void HammerHiddenViewportProxy::SetActiveViewport(HammerWidget* activeViewport)
    {
        m_activeViewport = activeViewport;
    }

    void HammerHiddenViewportProxy::SyncToActive()
    {
        if (!m_hiddenRealViewport || !m_activeViewport)
        {
            return;
        }
        AZ_Assert(m_gridContainer, "m_gridContainer must exist by the time viewports are being tracked");
        AZ_Assert(
            m_hiddenRealViewport->parentWidget() == m_gridContainer,
            "m_hiddenRealViewport is expected to remain parented to m_gridContainer - geometry below is computed relative to it");

        static constexpr int OffScreenCoordinate = -10000;
        m_hiddenRealViewport->setGeometry(
            OffScreenCoordinate, OffScreenCoordinate, m_activeViewport->width(), m_activeViewport->height());

        AtomToolsFramework::RenderViewportWidget* activeViewportWidget = m_activeViewport->GetViewportWidget();
        AZ_Assert(activeViewportWidget, "The active HammerWidget must have a valid RenderViewportWidget by now");
        if (m_hiddenRealViewportContext && activeViewportWidget)
        {
            AZ::RPI::ViewportContextPtr activeContext = activeViewportWidget->GetViewportContext();
            if (activeContext)
            {
                m_hiddenRealViewportContext->SetCameraTransform(activeContext->GetCameraTransform());
            }
        }

        m_viewportUiOverlayWindow.Get(
            [this]() -> QWidget*
            {
                QWidget* found = m_hiddenRealViewport->findChild<QWidget*>(QStringLiteral("ViewportUiWindow"));
                if (found)
                {
                    found->installEventFilter(this);
                }
                return found;
            });

        m_realInternalRenderViewport.Get(
            [this]() -> AtomToolsFramework::RenderViewportWidget*
            {
                QWidget* found = FindDescendantByClassName(m_hiddenRealViewport, "RenderViewportWidget");
                if (!found)
                {
                    return nullptr;
                }
                auto* renderViewport = static_cast<AtomToolsFramework::RenderViewportWidget*>(found);
                renderViewport->SetInputProcessingEnabled(false);
                return renderViewport;
            });
    }

    bool HammerHiddenViewportProxy::eventFilter(QObject* watched, QEvent* event)
    {
        if (watched == m_viewportUiOverlayWindow.Peek() && m_activeViewport &&
            (event->type() == QEvent::Move || event->type() == QEvent::Resize))
        {
            const QPoint desiredTopLeft = m_activeViewport->mapToGlobal(QPoint(0, 0));
            if (m_viewportUiOverlayWindow.Peek()->pos() != desiredTopLeft)
            {
                m_viewportUiOverlayWindow.Peek()->move(desiredTopLeft);
            }
        }
        return QObject::eventFilter(watched, event);
    }
}
