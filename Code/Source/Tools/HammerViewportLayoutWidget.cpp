#include "HammerViewportLayoutWidget.h"
#include "HammerWidget.h"

#include <AzCore/std/algorithm.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
                    });
            }

            // Seeds the initially-active viewport; every other slot already defaults to inactive.
            m_viewports[0]->SetActive(true);
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
        // m_gridLayout - it's not shown in any slot, ever. See the header comment on
        // SetHiddenRealViewport() for why keeping the object alive (not necessarily visible)
        // still matters.
        realViewport->setParent(m_gridContainer);
        realViewport->hide();
        m_hiddenRealViewport = realViewport;

        // The real viewport's AZ::RPI::ViewportContext was created first (at Editor startup, long
        // before any Hammer viewport exists), so it still holds the reserved "default" context
        // name/designation. AZ::RPI::ViewportContextManager::RenameViewportContext() refuses to
        // reassign a name that's already claimed (asserts and silently no-ops) - so as long as the
        // real viewport keeps squatting that name, HammerWidget::ApplyActiveState()'s attempt to
        // claim it for the active Hammer viewport can never succeed, and anything reading
        // GetDefaultViewportContext() (e.g. the FPS/resolution debug text overlay) keeps reading
        // this now-hidden, no-longer-resized viewport's stale size. Freeing the name here, once,
        // lets the active-viewport claim in HammerWidget actually take effect.
        if (auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get())
        {
            AZ::RPI::ViewportContextPtr defaultContext = viewportContextManager->GetDefaultViewportContext();
            AZ_Error(
                "HammerViewportLayoutWidget", defaultContext,
                "Could not find the current default ViewportContext to free its name from the real viewport");
            if (defaultContext)
            {
                viewportContextManager->RenameViewportContext(defaultContext, AZ::Name("Hammer Hidden Perspective Viewport"));
            }
        }
    }
} // namespace Hammer
