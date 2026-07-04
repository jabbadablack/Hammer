#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include "HammerLazyFind.h"

#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

class QGridLayout;
class QTimer;

namespace Hammer
{
    class HammerWidget;
    class ActiveViewportTracker;

    class HammerViewportLayoutWidget
        : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override = default;

        void AdoptRealPerspectiveViewport(QWidget& realViewport);
        void SetViewportCount(int count);
        void ToggleMaximizeActiveViewport();

    Q_SIGNALS:
        void ViewportCountChanged(int count);

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void RestoreMaximizeSwap();
        void MaximizeActiveViewport();
        void RestoreFromMaximize();
        void ActivateViewport(HammerWidget* viewport);
        void ReconcileGridSlots(int shownCount, int columns);
        void ResolveViewportUiOverlayWindow();
        void SyncViewportUiOverlay();

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<HammerWidget*> m_viewports;
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
        HammerWidget* m_activeViewport = nullptr;
        HammerWidget* m_adoptedViewport = nullptr;
        LazyFind<QWidget> m_viewportUiOverlayWindow;
        QTimer* m_overlaySyncTimer = nullptr;
        AZStd::array<HammerWidget*, MaxViewportCount> m_gridSlotWidget = {};
        AZStd::optional<int> m_maximizedFromIndex;
        int m_preMaximizeViewportCount = MinViewportCount;
        int m_currentViewportCount = MinViewportCount;
    };
} // namespace Hammer
