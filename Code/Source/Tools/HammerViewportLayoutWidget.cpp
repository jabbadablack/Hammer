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
                    for (HammerWidget* sibling : m_viewports)
                    {
                        sibling->SetActive(sibling == viewport);
                    }
                    m_hiddenViewportProxy->SetActiveViewport(*viewport);
                });
        }

        m_viewports[0]->SetActive(true);
        m_hiddenViewportProxy->SetActiveViewport(*m_viewports[0]);

        SetViewportCount(1);
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
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

        m_currentViewportCount = count;
        emit ViewportCountChanged(count);
    }

    void HammerViewportLayoutWidget::SetHiddenRealViewport(QWidget& realViewport)
    {
        m_hiddenViewportProxy->SetHiddenRealViewport(realViewport);
    }

    void HammerViewportLayoutWidget::RestoreMaximizeSwap()
    {
        m_maximizedFromIndex.has_value() && (AZStd::swap(m_viewports[0], m_viewports[*m_maximizedFromIndex]), true);
        m_maximizedFromIndex.reset();
    }

    void HammerViewportLayoutWidget::MaximizeActiveViewport()
    {
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
        const int restoreCount = m_preMaximizeViewportCount;
        RestoreMaximizeSwap();
        SetViewportCount(restoreCount);
    }

    void HammerViewportLayoutWidget::ToggleMaximizeActiveViewport()
    {
        using Action = void (HammerViewportLayoutWidget::*)();
        static constexpr AZStd::array<Action, 2> Actions = {
            &HammerViewportLayoutWidget::MaximizeActiveViewport, &HammerViewportLayoutWidget::RestoreFromMaximize
        };
        (this->*Actions[static_cast<size_t>(m_maximizedFromIndex.has_value())])();
    }
} // namespace Hammer
