#include "HammerViewportLayoutWidget.h"
#include "HammerWidget.h"

#include <AzCore/std/algorithm.h>

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
    }
} // namespace Hammer
