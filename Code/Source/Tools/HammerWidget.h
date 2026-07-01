
#pragma once

#if !defined(Q_MOC_RUN)
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QWidget>
#endif

namespace Hammer
{
    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        explicit HammerWidget(QWidget* parent = nullptr);
    };
}
