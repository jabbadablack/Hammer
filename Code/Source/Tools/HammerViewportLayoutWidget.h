
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/containers/vector.h>

class QGridLayout;
class QPushButton;

namespace Hammer
{
    class HammerWidget;

    // Hosts 1-4 Hammer-owned viewports arranged in a grid, with a small toolbar to switch the
    // count. All viewports are HammerWidget instances: slot 0 has its own camera controller
    // (matching the default Editor viewport's feel), and any additional slots (1-3) mirror slot
    // 0's camera instead of navigating independently. Selection and gizmos on every slot come from
    // HammerViewportManipulatorController, which uses only public AzToolsFramework APIs.
    class HammerViewportLayoutWidget
        : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override = default;

    private:
        void SetViewportCount(int count);

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<QPushButton*> m_countButtons;
        AZStd::vector<HammerWidget*> m_viewports;
    };
} // namespace Hammer
