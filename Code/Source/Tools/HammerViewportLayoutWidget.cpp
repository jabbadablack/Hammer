#include "HammerViewportLayoutWidget.h"
#include "HammerActiveViewportTracker.h"
#include "HammerWidget.h"

#include "HammerOptionalUtils.h"

#include <Hammer/IHammerQtEnvironment.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>

#include <QEvent>
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

        const bool alreadyActive = viewport == m_activeViewport;

        (!alreadyActive && m_activeViewport) && (m_activeViewport->SetActive(false), true);
        !alreadyActive && (viewport->SetActive(true), m_activeViewport = viewport, true);
    }

    void HammerViewportLayoutWidget::ReconcileGridSlots(int shownCount, int columns)
    {
        AZ_Assert(
            shownCount >= 0 && shownCount <= MaxViewportCount, "ReconcileGridSlots given an out-of-range shownCount %d", shownCount);
        AZ_Assert(columns == 1 || columns == 2, "ReconcileGridSlots given an unexpected column count %d", columns);

        AZStd::array<HammerWidget*, MaxViewportCount> desiredSlotWidget = {};
        for (int i = 0; i < shownCount; ++i)
        {
            desiredSlotWidget[i] = m_viewports[i];
        }

        for (HammerWidget* viewport : m_viewports)
        {
            const auto oldIt = AZStd::find(m_gridSlotWidget.begin(), m_gridSlotWidget.end(), viewport);
            const auto newIt = AZStd::find(desiredSlotWidget.begin(), desiredSlotWidget.end(), viewport);

            const bool wasShown = oldIt != m_gridSlotWidget.end();
            const bool willShow = newIt != desiredSlotWidget.end();
            const int oldSlot = static_cast<int>(AZStd::distance(m_gridSlotWidget.begin(), oldIt));
            const int newSlot = static_cast<int>(AZStd::distance(desiredSlotWidget.begin(), newIt));
            const bool unchanged = wasShown && willShow && (oldSlot == newSlot);

            (wasShown && !unchanged) && (m_gridLayout->removeWidget(viewport), true);
            (wasShown && !willShow) && (viewport->hide(), viewport->SetRenderTickEnabled(false), true);
            (willShow && !unchanged) &&
                (m_gridLayout->addWidget(viewport, newSlot / columns, newSlot % columns), viewport->show(),
                 viewport->SetRenderTickEnabled(true), true);
        }

        m_gridSlotWidget = desiredSlotWidget;
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        AZ_Assert(m_gridLayout, "SetViewportCount called before the grid layout was constructed");
        AZ_Assert(m_viewports.size() == MaxViewportCount, "SetViewportCount expects all %d viewports to already exist", MaxViewportCount);

        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);

        (count != 1) && (RestoreMaximizeSwap(), true);

        const int columns = 1 + static_cast<int>(count > 1);
        const int shownCount = AZStd::GetMin(count, static_cast<int>(m_viewports.size()));

        ReconcileGridSlots(shownCount, columns);

        ActivateViewport(m_viewports[0]);

        m_currentViewportCount = count;
        emit ViewportCountChanged(count);
    }

    void HammerViewportLayoutWidget::AdoptRealPerspectiveViewport(QWidget& realViewport)
    {
        AZ_Assert(!m_adoptedViewport, "AdoptRealPerspectiveViewport called more than once");
        AZ_Assert(
            m_viewports.size() == MaxViewportCount, "AdoptRealPerspectiveViewport expects all %d viewports to already exist",
            MaxViewportCount);
        AZ_Assert(m_activeViewport == m_viewports[0], "AdoptRealPerspectiveViewport expects slot 0 to still be the active viewport");

        HammerWidget* placeholder = m_viewports[0];

        HammerWidget* adopted = HammerWidget::CreateAdopting(m_gridContainer, realViewport);
        AZ_Assert(adopted, "Failed to allocate the adopting HammerWidget");

        connect(
            adopted, &HammerWidget::ViewportFocusRequested, this,
            [this, adopted]
            {
                ActivateViewport(adopted);
            });

        m_viewports[0] = adopted;
        m_adoptedViewport = adopted;

        const auto slotIt = AZStd::find(m_gridSlotWidget.begin(), m_gridSlotWidget.end(), placeholder);
        (slotIt != m_gridSlotWidget.end()) && (*slotIt = adopted, true);

        m_gridLayout->removeWidget(placeholder);
        placeholder->hide();
        placeholder->SetRenderTickEnabled(false);

        m_gridLayout->addWidget(adopted, 0, 0);
        adopted->show();
        adopted->SetRenderTickEnabled(true);

        ActivateViewport(adopted);

        delete placeholder;

        ResolveViewportUiOverlayWindow();
    }

    void HammerViewportLayoutWidget::ResolveViewportUiOverlayWindow()
    {
        AZ_Assert(m_adoptedViewport, "ResolveViewportUiOverlayWindow called before the real viewport was adopted");

        auto* qtEnvironment = AZ::Interface<IHammerQtEnvironment>::Get();
        AZ_Assert(qtEnvironment, "IHammerQtEnvironment must be registered before resolving the ViewportUi overlay window");

        QWidget* overlay = m_viewportUiOverlayWindow.Get(
            [this, qtEnvironment]() -> QWidget*
            {
                return qtEnvironment->FindViewportUiOverlayWindow(m_adoptedViewport);
            });
        overlay && (overlay->installEventFilter(this), true);
    }

    bool HammerViewportLayoutWidget::eventFilter(QObject* watched, QEvent* event)
    {
        const bool isOverlayMoveOrResize = watched == m_viewportUiOverlayWindow.Peek() && m_activeViewport &&
            m_activeViewport != m_adoptedViewport && (event->type() == QEvent::Move || event->type() == QEvent::Resize);

        isOverlayMoveOrResize &&
            (static_cast<QWidget*>(watched)->setGeometry(
                 QRect(m_activeViewport->mapToGlobal(QPoint(0, 0)), m_activeViewport->size())),
             true);

        return QWidget::eventFilter(watched, event);
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
        AZ_Assert(m_activeViewport, "MaximizeActiveViewport called before any viewport was ever activated");

        const auto it = AZStd::find(m_viewports.begin(), m_viewports.end(), m_activeViewport);
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
