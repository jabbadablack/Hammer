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
        for (int count = MinViewportCount; count <= MaxViewportCount; ++count)
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

    QWidget* HammerViewportLayoutWidget::SlotWidget(int index) const
    {
        // The real viewport override is only usable in solo (1-viewport) mode: reusing it inside a
        // subdivided grid cell repeatedly failed to respect the grid's geometry (it kept rendering
        // as if it still owned the whole container, regardless of the small cell QGridLayout
        // assigned it - a native-rendering-surface quirk specific to the real Editor viewport that
        // HammerWidget's own viewports don't share, and not something fixable without Editor-private
        // code changes). Falling back to the normal HammerWidget for slot 0 whenever more than one
        // viewport is shown avoids that entirely.
        if (index == 0 && m_primaryViewportOverride && m_currentCount == 1)
        {
            return m_primaryViewportOverride;
        }
        return m_viewports[index];
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);
        m_currentCount = count;

        // Viewports are created once (below) and never destroyed for the lifetime of this widget;
        // switching the count only shows/hides and repositions them. Destroying a HammerWidget
        // tears down its Atom render pipeline (via ~ViewportContext -> Scene::RemoveRenderPipeline),
        // which triggers the scene's whole pass tree to rebuild - and doing that while other
        // viewports/pipelines are still around crashed the Editor (LightCullingPass/
        // ReflectionScreenSpaceTracePass hitting a zero-sized buffer during that rebuild). This
        // also applies to a superseded slot-0 HammerWidget once SetPrimaryViewportOverride() has
        // replaced it - it stays alive, just hidden, forever.
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
        if (m_primaryViewportOverride)
        {
            m_gridLayout->removeWidget(m_primaryViewportOverride);
            m_primaryViewportOverride->hide();
        }

        // 1 viewport -> single cell; 2 -> side by side; 3/4 -> 2x2 grid (3 leaves the last cell empty).
        const int columns = count <= 1 ? 1 : 2;
        const int shownCount = AZStd::GetMin(count, static_cast<int>(m_viewports.size()));
        for (int i = 0; i < shownCount; ++i)
        {
            QWidget* slotWidget = SlotWidget(i);
            m_gridLayout->addWidget(slotWidget, i / columns, i % columns);
            slotWidget->show();
        }

        for (int i = 0; i < static_cast<int>(m_countButtons.size()); ++i)
        {
            m_countButtons[i]->setChecked(i + MinViewportCount == count);
        }
    }

    void HammerViewportLayoutWidget::SetPrimaryViewportOverride(QWidget* realViewport)
    {
        AZ_Assert(realViewport, "SetPrimaryViewportOverride called with a null widget");
        if (!realViewport)
        {
            return;
        }

        realViewport->setParent(m_gridContainer);
        m_primaryViewportOverride = realViewport;
        SetViewportCount(m_currentCount);
    }
} // namespace Hammer
