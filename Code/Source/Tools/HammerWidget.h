
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

namespace Hammer
{
    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        // wireframe enables Hammer's wireframe overlay on top of the normal default rendering.
        explicit HammerWidget(bool wireframe = true, QWidget* parent = nullptr);
        ~HammerWidget() override = default;
    };
}
