
#pragma once

#if !defined(Q_MOC_RUN)
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <Atom/RPI.Public/Base.h>

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
        ~HammerWidget() override;

    private:
        AZ::RPI::RenderPipelinePtr m_pipeline;
    };
}
