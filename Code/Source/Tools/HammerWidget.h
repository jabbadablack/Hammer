
#pragma once

#if !defined(Q_MOC_RUN)
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <Atom/RPI.Public/Base.h>
#include <AzFramework/Viewport/ViewportId.h>

#include <QWidget>
#endif

class QPushButton;
class QResizeEvent;

namespace Hammer
{
    class HammerWidget
        : public QWidget
    {
    Q_OBJECT
    public:
        explicit HammerWidget(QWidget* parent = nullptr);
        ~HammerWidget() override;

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        void FollowMainViewportCamera();
        void RepositionFollowCameraButton();

        AZ::RPI::RenderPipelinePtr m_pipeline;
        AzFramework::ViewportId m_viewportId = AzFramework::InvalidViewportId;
        QPushButton* m_followMainCameraButton = nullptr;
    };
}
