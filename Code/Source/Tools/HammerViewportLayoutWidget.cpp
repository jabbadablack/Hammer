#include "HammerViewportLayoutWidget.h"
#include "HammerActiveViewportTracker.h"
#include "HammerHiddenViewportProxy.h"
#include "HammerWidget.h"

#include "HammerOptionalUtils.h"

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>

#include <QGridLayout>
#include <QVBoxLayout>

namespace Hammer
{
    HammerViewportLayoutWidget::HammerViewportLayoutWidget(QWidget* parent)
        : QWidget(parent)
        , m_activeViewportTracker(AZStd::make_shared<ActiveViewportTracker>())
    {
        QVBoxLayout* outerLayout = new QVBoxLayout(this);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);

        m_gridContainer = new QWidget(this);
        m_gridLayout = new QGridLayout(m_gridContainer);
        m_gridLayout->setContentsMargins(0, 0, 0, 0);
        m_gridLayout->setSpacing(3);
        outerLayout->addWidget(m_gridContainer, /*stretch*/ 1);

        m_hiddenViewportProxy = new HammerHiddenViewportProxy(*m_gridContainer, this);
        AZ_Assert(m_hiddenViewportProxy, "Failed to allocate HammerHiddenViewportProxy");

        for (int i = 0; i < MaxViewportCount; ++i)
        {
            HammerWidget* viewport = new HammerWidget(m_gridContainer);
            AZ_Assert(viewport, "Failed to allocate HammerWidget #%d", i);
            m_viewports.push_back(viewport);
        }

        for (HammerWidget* viewport : m_viewports)
        {
            connect(
                viewport, &HammerWidget::ViewportFocusRequested, this,
                [this, viewport]
                {
                    ActivateViewport(viewport);
                });
        }

        SetViewportCount(1);
    }

    void HammerViewportLayoutWidget::ActivateViewport(HammerWidget* viewport)
    {
        AZ_Assert(viewport, "ActivateViewport called with a null viewport");
        AZ_Assert(!m_viewports.empty(), "ActivateViewport called with no viewports");

        for (HammerWidget* sibling : m_viewports)
        {
            (sibling != viewport) && (sibling->SetActive(false), true);
        }
        viewport->SetActive(true);
        m_hiddenViewportProxy->SetActiveViewport(*viewport);
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        AZ_Assert(m_gridLayout, "SetViewportCount called before the grid layout was constructed");
        AZ_Assert(m_viewports.size() == MaxViewportCount, "SetViewportCount expects all %d viewports to already exist", MaxViewportCount);

        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);

        (count != 1) && (RestoreMaximizeSwap(), true);

        for (HammerWidget* viewport : m_viewports)
        {
            m_gridLayout->removeWidget(viewport);
            viewport->hide();
            viewport->SetRenderTickEnabled(false);
        }

        const int columns = 1 + static_cast<int>(count > 1);
        const int shownCount = AZStd::GetMin(count, static_cast<int>(m_viewports.size()));
        for (int i = 0; i < shownCount; ++i)
        {
            m_gridLayout->addWidget(m_viewports[i], i / columns, i % columns);
            m_viewports[i]->show();
            m_viewports[i]->SetRenderTickEnabled(true);
        }

        ActivateViewport(m_viewports[0]);

        m_currentViewportCount = count;
        emit ViewportCountChanged(count);
    }

    void HammerViewportLayoutWidget::SetHiddenRealViewport(QWidget& realViewport)
    {
        AZ_Assert(m_hiddenViewportProxy, "SetHiddenRealViewport called before the hidden viewport proxy was constructed");
        m_hiddenViewportProxy->SetHiddenRealViewport(realViewport);
    }

    void HammerViewportLayoutWidget::RestoreMaximizeSwap()
    {
        AZ_Assert(
            !m_maximizedFromIndex.has_value() || *m_maximizedFromIndex < static_cast<int>(m_viewports.size()),
            "RestoreMaximizeSwap has an out-of-range maximized index %d", m_maximizedFromIndex.value_or(-1));
        m_maximizedFromIndex.has_value() && (AZStd::swap(m_viewports[0], m_viewports[*m_maximizedFromIndex]), true);
        m_maximizedFromIndex.reset();
    }

    void HammerViewportLayoutWidget::MaximizeActiveViewport()
    {
        AZ_Assert(!m_viewports.empty(), "MaximizeActiveViewport called with no viewports to maximize");
        AZ_Assert(!m_maximizedFromIndex.has_value(), "MaximizeActiveViewport called while already maximized");

        const AzFramework::ViewportId activeId = m_activeViewportTracker->GetActiveViewportId();
        const auto it = AZStd::find_if(
            m_viewports.begin(), m_viewports.end(),
            [activeId](HammerWidget* viewport)
            {
                return viewport->GetViewportWidget() && viewport->GetViewportWidget()->GetId() == activeId;
            });
        const int activeIndex =
            OptionalUtils::OptionalWhen(it != m_viewports.end(), static_cast<int>(AZStd::distance(m_viewports.begin(), it)))
                .value_or(0);

        m_preMaximizeViewportCount = m_currentViewportCount;
        m_maximizedFromIndex = activeIndex;
        AZStd::swap(m_viewports[0], m_viewports[activeIndex]);

        SetViewportCount(1);
    }

    void HammerViewportLayoutWidget::RestoreFromMaximize()
    {
        AZ_Assert(m_maximizedFromIndex.has_value(), "RestoreFromMaximize called while not maximized");
        AZ_Assert(
            m_preMaximizeViewportCount >= MinViewportCount && m_preMaximizeViewportCount <= MaxViewportCount,
            "RestoreFromMaximize has an out-of-range pre-maximize viewport count %d", m_preMaximizeViewportCount);

        const int restoreCount = m_preMaximizeViewportCount;
        RestoreMaximizeSwap();
        SetViewportCount(restoreCount);
    }

    void HammerViewportLayoutWidget::ToggleMaximizeActiveViewport()
    {
        AZ_Assert(!m_viewports.empty(), "ToggleMaximizeActiveViewport called with no viewports");

        using Action = void (HammerViewportLayoutWidget::*)();
        static constexpr AZStd::array<Action, 2> Actions = {
            &HammerViewportLayoutWidget::MaximizeActiveViewport, &HammerViewportLayoutWidget::RestoreFromMaximize
        };
        (this->*Actions[static_cast<size_t>(m_maximizedFromIndex.has_value())])();
    }
} // namespace Hammer
