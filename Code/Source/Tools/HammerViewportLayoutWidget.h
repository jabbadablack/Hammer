
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

class QGridLayout;

namespace Hammer
{
    class HammerWidget;
    class ActiveViewportTracker;
    class HammerHiddenViewportProxy;

    class HammerViewportLayoutWidget
        : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override = default;

        void SetHiddenRealViewport(QWidget* realViewport);
        void SetViewportCount(int count);
        void ToggleMaximizeActiveViewport();

    Q_SIGNALS:
        void ViewportCountChanged(int count);

    private:
        void RestoreMaximizeSwap();

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<HammerWidget*> m_viewports;
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
        HammerHiddenViewportProxy* m_hiddenViewportProxy = nullptr;
        bool m_isMaximized = false;
        int m_maximizedFromIndex = -1;
        int m_preMaximizeViewportCount = MinViewportCount;
        int m_currentViewportCount = MinViewportCount;
    };
} // namespace Hammer
