
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

    // Hosts 1-4 viewports arranged in a grid, with a small toolbar to switch the count. Slots are
    // HammerWidget instances (each with its own independent camera controller, matching the
    // default Editor viewport's feel), except slot 0 when in solo (1-viewport) mode and
    // SetPrimaryViewportOverride() has been given a real, externally-owned Editor viewport widget
    // to show instead - see SlotWidget() for why this is solo-mode-only. Selection and gizmos on
    // HammerWidget slots come from HammerViewportManipulatorController, which uses only public
    // AzToolsFramework APIs; the real viewport widget (when shown) already has its own native
    // selection/gizmo handling.
    class HammerViewportLayoutWidget
        : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override = default;

        // Makes realViewport available as grid slot 0's widget while in solo (1-viewport) mode
        // (see SlotWidget()); at any other viewport count, slot 0 keeps using its normal
        // HammerWidget, which is never destroyed - see the "create once, never destroy" comment in
        // SetViewportCount().
        void SetPrimaryViewportOverride(QWidget* realViewport);

    private:
        void SetViewportCount(int count);
        QWidget* SlotWidget(int index) const;

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<QPushButton*> m_countButtons;
        AZStd::vector<HammerWidget*> m_viewports;
        QWidget* m_primaryViewportOverride = nullptr;
        int m_currentCount = MaxViewportCount;
    };
} // namespace Hammer
