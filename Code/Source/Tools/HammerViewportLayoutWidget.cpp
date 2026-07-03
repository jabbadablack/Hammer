#include "HammerViewportLayoutWidget.h"
#include "HammerActiveViewportTracker.h"
#include "HammerHiddenViewportProxy.h"
#include "HammerWidget.h"

#include <AzCore/std/algorithm.h>
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

        m_hiddenViewportProxy = new HammerHiddenViewportProxy(m_gridContainer, this);
        AZ_Assert(m_hiddenViewportProxy, "Failed to allocate HammerHiddenViewportProxy");

        SetViewportCount(1);
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);

        if (count != 1)
        {
            RestoreMaximizeSwap();
        }

        if (m_viewports.empty())
        {
            for (int i = 0; i < MaxViewportCount; ++i)
            {
                HammerWidget* viewport = new HammerWidget(m_gridContainer, m_activeViewportTracker);
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
                            if (sibling != viewport)
                            {
                                sibling->SetActive(false);
                            }
                        }
                        viewport->SetActive(true);
                        m_hiddenViewportProxy->SetActiveViewport(viewport);
                    });
            }

            m_viewports[0]->SetActive(true);
            m_hiddenViewportProxy->SetActiveViewport(m_viewports[0]);
        }

        for (HammerWidget* viewport : m_viewports)
        {
            m_gridLayout->removeWidget(viewport);
            viewport->hide();
            viewport->SetRenderTickEnabled(false);
        }

        const int columns = count <= 1 ? 1 : 2;
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

    void HammerViewportLayoutWidget::SetHiddenRealViewport(QWidget* realViewport)
    {
        m_hiddenViewportProxy->SetHiddenRealViewport(realViewport);
    }

    void HammerViewportLayoutWidget::RestoreMaximizeSwap()
    {
        if (!m_isMaximized)
        {
            return;
        }

        if (m_maximizedFromIndex > 0 && m_maximizedFromIndex < static_cast<int>(m_viewports.size()))
        {
            AZStd::swap(m_viewports[0], m_viewports[m_maximizedFromIndex]);
        }
        m_maximizedFromIndex = -1;
        m_isMaximized = false;
    }

    void HammerViewportLayoutWidget::ToggleMaximizeActiveViewport()
    {
        if (m_isMaximized)
        {
            const int restoreCount = m_preMaximizeViewportCount;
            RestoreMaximizeSwap();
            SetViewportCount(restoreCount);
            return;
        }

        if (m_viewports.empty())
        {
            return;
        }

        int activeIndex = 0;
        if (m_activeViewportTracker)
        {
            const AzFramework::ViewportId activeId = m_activeViewportTracker->GetActiveViewportId();
            for (int i = 0; i < static_cast<int>(m_viewports.size()); ++i)
            {
                if (m_viewports[i]->GetViewportWidget() && m_viewports[i]->GetViewportWidget()->GetId() == activeId)
                {
                    activeIndex = i;
                    break;
                }
            }
        }

        m_preMaximizeViewportCount = m_currentViewportCount;
        m_maximizedFromIndex = activeIndex > 0 ? activeIndex : -1;
        m_isMaximized = true;
        if (activeIndex > 0)
        {
            AZStd::swap(m_viewports[0], m_viewports[activeIndex]);
        }

        SetViewportCount(1);
    }
} // namespace Hammer
