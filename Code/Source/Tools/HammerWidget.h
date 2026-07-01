
#pragma once

#if !defined(Q_MOC_RUN)
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
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
        // wireframe enables Hammer's wireframe overlay on top of the normal default rendering.
        explicit HammerWidget(bool wireframe = true, QWidget* parent = nullptr);
        ~HammerWidget() override = default;

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        void FollowMainViewportCamera();
        void RepositionFollowCameraButton();

        AzFramework::ViewportId m_viewportId = AzFramework::InvalidViewportId;
        QPushButton* m_followMainCameraButton = nullptr;
    };
}
