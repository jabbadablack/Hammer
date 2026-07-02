#include "HammerViewportLayoutWidget.h"
#include "HammerWidget.h"

#include <AzCore/std/algorithm.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace Hammer
{
    HammerViewportLayoutWidget::HammerViewportLayoutWidget(QWidget* parent)
        : QWidget(parent)
    {
        QVBoxLayout* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);

        QHBoxLayout* toolbarLayout = new QHBoxLayout();
        toolbarLayout->addWidget(new QLabel(tr("Viewports:"), this));
        // 3 is deliberately omitted: a 2x2 grid with one empty cell looked bad, so the only
        // selectable counts are 1 (solo), 2 (side by side), and 4 (full 2x2 grid).
        for (int count : { 1, 2, 4 })
        {
            QPushButton* button = new QPushButton(QString::number(count), this);
            button->setCheckable(true);
            button->setFixedWidth(24);
            connect(button, &QPushButton::clicked, this, [this, count] { SetViewportCount(count); });
            toolbarLayout->addWidget(button);
            m_countButtons.push_back(button);
        }
        toolbarLayout->addStretch();
        outerLayout->addLayout(toolbarLayout);

        m_gridContainer = new QWidget(this);
        m_gridLayout = new QGridLayout(m_gridContainer);
        m_gridLayout->setContentsMargins(0, 0, 0, 0);
        m_gridLayout->setSpacing(0);
        outerLayout->addWidget(m_gridContainer, /*stretch*/ 1);

        SetViewportCount(4);

        m_realViewportSyncTimer = new QTimer(this);
        connect(m_realViewportSyncTimer, &QTimer::timeout, this, &HammerViewportLayoutWidget::SyncRealViewportToActive);
        m_realViewportSyncTimer->start(100);
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);

        // Viewports are created once (below) and never destroyed for the lifetime of this widget;
        // switching the count only shows/hides and repositions them. Destroying a HammerWidget
        // tears down its Atom render pipeline (via ~ViewportContext -> Scene::RemoveRenderPipeline),
        // which triggers the scene's whole pass tree to rebuild - and doing that while other
        // viewports/pipelines are still around crashed the Editor (LightCullingPass/
        // ReflectionScreenSpaceTracePass hitting a zero-sized buffer during that rebuild).
        if (m_viewports.empty())
        {
            for (int i = 0; i < MaxViewportCount; ++i)
            {
                // Every slot gets its own independent camera controller.
                HammerWidget* viewport = new HammerWidget(m_gridContainer);
                AZ_Assert(viewport, "Failed to allocate HammerWidget #%d", i);
                m_viewports.push_back(viewport);
            }

            // Only one viewport is ever "active" (input-processing enabled) at a time - see the
            // class comment on HammerWidget for why. Whichever viewport's RenderViewportWidget
            // gains Qt focus (e.g. the user clicks into it) becomes the new active one; every
            // sibling is deactivated in response.
            for (HammerWidget* viewport : m_viewports)
            {
                connect(
                    viewport, &HammerWidget::ViewportFocusRequested, this,
                    [this, viewport]
                    {
                        for (HammerWidget* sibling : m_viewports)
                        {
                            sibling->SetActive(sibling == viewport);
                        }
                        m_activeViewport = viewport;
                    });
            }

            // Seeds the initially-active viewport; every other slot already defaults to inactive.
            m_viewports[0]->SetActive(true);
            m_activeViewport = m_viewports[0];
        }

        for (HammerWidget* viewport : m_viewports)
        {
            m_gridLayout->removeWidget(viewport);
            viewport->hide();
        }

        // 1 viewport -> single cell; 2 -> side by side; 4 -> full 2x2 grid.
        const int columns = count <= 1 ? 1 : 2;
        const int shownCount = AZStd::GetMin(count, static_cast<int>(m_viewports.size()));
        for (int i = 0; i < shownCount; ++i)
        {
            m_gridLayout->addWidget(m_viewports[i], i / columns, i % columns);
            m_viewports[i]->show();
        }

        // Matches the { 1, 2, 4 } counts the toolbar buttons were built from, in order.
        int buttonIndex = 0;
        for (int buttonCount : { 1, 2, 4 })
        {
            m_countButtons[buttonIndex]->setChecked(buttonCount == count);
            ++buttonIndex;
        }
    }

    void HammerViewportLayoutWidget::SetHiddenRealViewport(QWidget* realViewport)
    {
        AZ_Assert(realViewport, "SetHiddenRealViewport called with a null widget");
        if (!realViewport)
        {
            return;
        }

        // Parented here so it's kept alive for this widget's whole lifetime, but never added to
        // m_gridLayout - it's not shown in any slot, ever, all of which always use HammerWidget.
        // See the header comment on SetHiddenRealViewport() for the full rationale. The real
        // viewport's default-ViewportContext-name handoff to Hammer's own viewports happens
        // earlier, in HammerEditorSystemComponent::EmbedViewportInCenter() - before this function
        // is even called - see the comment there for why the timing matters.
        AZ_Assert(m_gridContainer, "m_gridContainer must be constructed before hosting the real viewport");
        realViewport->setParent(m_gridContainer);

        // Genuinely Qt-visible (not hidden) - confirmed by direct source reading that
        // EditorViewportWidget::Update() (Code/Editor/EditorViewportWidget.cpp:426-429)
        // unconditionally early-returns on !isVisible(), and that's what drives its ViewportUi
        // overlay (the transform-mode/reference-space/component-mode widgets this whole thing is
        // for).
        //
        // NOT positioned over the active Hammer viewport, though - an earlier attempt did that
        // (plus WA_TransparentForMouseEvents/NoFocus to try to stop it stealing input) and camera
        // navigation still intermittently broke: a real, fully-interactive native-surface widget
        // sitting in the same screen space as the Hammer viewport underneath it was disruptive
        // regardless (this codebase has repeatedly seen native/GPU-surface widgets not fully
        // respect normal Qt input-routing attributes). SyncRealViewportToActive() instead keeps
        // this positioned off-screen entirely - genuinely visible, so Update() keeps running, but
        // nowhere the user can ever see or interact with it directly - and separately forces just
        // its ViewportUi overlay window (a different, independently-positioned widget) back onto
        // the active Hammer viewport via eventFilter(). Kept mouse/keyboard-transparent regardless,
        // as a cheap defensive margin in case it's ever transiently repositioned on-screen.
        realViewport->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        realViewport->setFocusPolicy(Qt::NoFocus);
        realViewport->show();
        m_hiddenRealViewport = realViewport;

        if (auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get())
        {
            m_hiddenRealViewportContext = viewportContextManager->GetViewportContextByName(AZ::Name("Hammer Hidden Perspective Viewport"));
            AZ_Error(
                "HammerViewportLayoutWidget", m_hiddenRealViewportContext,
                "Could not resolve the real viewport's own ViewportContext by name - camera syncing will not work");
        }
    }

    void HammerViewportLayoutWidget::SyncRealViewportToActive()
    {
        if (!m_hiddenRealViewport || !m_activeViewport)
        {
            return;
        }
        AZ_Assert(m_gridContainer, "m_gridContainer must exist by the time viewports are being tracked");
        AZ_Assert(
            m_hiddenRealViewport->parentWidget() == m_gridContainer,
            "m_hiddenRealViewport is expected to remain parented to m_gridContainer - geometry below is computed relative to it");

        // Positioned far outside any conceivable monitor arrangement - genuinely visible (see
        // SetHiddenRealViewport()) but nowhere the user can see or click into it. Still sized to
        // match the active Hammer viewport, since its ViewportUi overlay derives its own size (used
        // for aligning clusters to the overlay's edges) from this widget's size, via a private code
        // path we can't call into directly - only the overlay's on-screen position gets forced
        // elsewhere, in eventFilter() below.
        static constexpr int OffScreenCoordinate = -10000;
        m_hiddenRealViewport->setGeometry(
            OffScreenCoordinate, OffScreenCoordinate, m_activeViewport->width(), m_activeViewport->height());

        // Keeps the real viewport's own camera matching the active Hammer viewport's. Not required
        // for the ViewportUi overlay itself, but kept as a defensive measure in case the real
        // viewport's own render ever becomes visible (e.g. a transient off-screen-positioning
        // glitch), so there's nothing for the user to perceive as a second, mismatched viewport.
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

        // The overlay window is created lazily by ViewportUiDisplay (only once Update() has had a
        // chance to run and something's actually been added to it), so it may not exist yet the
        // first several times this runs. Found once and cached; installEventFilter() only needs to
        // happen once too.
        if (!m_viewportUiOverlayWindow)
        {
            m_viewportUiOverlayWindow = m_hiddenRealViewport->findChild<QWidget*>(QStringLiteral("ViewportUiWindow"));
            if (m_viewportUiOverlayWindow)
            {
                m_viewportUiOverlayWindow->installEventFilter(this);
            }
        }
    }

    bool HammerViewportLayoutWidget::eventFilter(QObject* watched, QEvent* event)
    {
        // Every time ViewportUiDisplay::Update() runs (driven by the real viewport's own now-passing
        // Update() gate - see SetHiddenRealViewport()), its private PositionUiOverlayOverRenderViewport()
        // recomputes m_viewportUiOverlayWindow's geometry from the real viewport's own (off-screen -
        // see SyncRealViewportToActive()) position. Intercepting the resulting Move/Resize and
        // immediately correcting just the position (not the size, which is already right since the
        // real viewport is kept sized to match) is the only way to get the overlay to track the
        // active Hammer viewport instead, since there's no public API to redirect what geometry it
        // computes from in the first place. Guarded against infinite recursion: move() below
        // triggers another QEvent::Move, but by then pos() already matches desiredTopLeft, so the
        // inner recursive call takes the early-return branch instead of calling move() again.
        if (watched == m_viewportUiOverlayWindow && m_activeViewport &&
            (event->type() == QEvent::Move || event->type() == QEvent::Resize))
        {
            const QPoint desiredTopLeft = m_activeViewport->mapToGlobal(QPoint(0, 0));
            if (m_viewportUiOverlayWindow->pos() != desiredTopLeft)
            {
                m_viewportUiOverlayWindow->move(desiredTopLeft);
            }
        }
        return QWidget::eventFilter(watched, event);
    }
} // namespace Hammer
