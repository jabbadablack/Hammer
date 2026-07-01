
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

    // Hosts 1-4 HammerWidget viewports arranged in a grid, with a small toolbar to switch the
    // count. The first viewport has its own camera controller, matching the default Editor
    // viewport; any additional viewports have no camera input of their own and instead mirror
    // the first viewport's camera every time it changes (see HammerWidget's cameraSource param).
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
