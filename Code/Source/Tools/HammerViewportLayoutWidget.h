
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

class QGridLayout;
class QPushButton;

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

    private:
        void SetViewportCount(int count);

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<QPushButton*> m_countButtons;
        AZStd::vector<HammerWidget*> m_viewports;
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
        HammerHiddenViewportProxy* m_hiddenViewportProxy = nullptr;
    };
} // namespace Hammer
