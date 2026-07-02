#include "HammerViewportLayoutWidget.h"
#include "HammerWidget.h"

#include <AzCore/std/algorithm.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

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
        // for). SyncRealViewportToActive() (driven by m_realViewportSyncTimer, started in the
        // constructor) continuously repositions and camera-syncs this widget to the active Hammer
        // viewport, so the user perceives it as part of that viewport rather than a second,
        // independent one - see that function for the full rationale.
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

        // Keeps the real viewport positioned/sized exactly over the active Hammer viewport, so its
        // ViewportUi overlay (which derives its own position/size from this widget's geometry via
        // a private code path we can't call into directly) ends up in the right place. Coordinates
        // must be converted since m_hiddenRealViewport's Qt parent is m_gridContainer, not the
        // desktop.
        const QPoint activeGlobalTopLeft = m_activeViewport->mapToGlobal(QPoint(0, 0));
        const QPoint relativeTopLeft = m_gridContainer->mapFromGlobal(activeGlobalTopLeft);
        m_hiddenRealViewport->setGeometry(
            relativeTopLeft.x(), relativeTopLeft.y(), m_activeViewport->width(), m_activeViewport->height());

        // Keeps the real viewport's own camera matching the active Hammer viewport's, so even
        // though it's genuinely visible (see SetHiddenRealViewport()) and positioned directly over
        // the active viewport, its own render shows the identical scene from the identical angle -
        // nothing for the user to perceive as a second, competing viewport, regardless of how the
        // two native render surfaces end up Z-ordered relative to each other.
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
    }
} // namespace Hammer
