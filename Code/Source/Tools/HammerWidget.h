#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#include <Atom/RPI.Public/ViewportContext.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzCore/Component/EntityId.h>
#include <AzFramework/Viewport/ViewportControllerInterface.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#endif

class QResizeEvent;
class QShowEvent;
class QEvent;
class QObject;

namespace Hammer
{
    struct HammerViewModes
    {
        bool m_normal = true;
        bool m_wireframe = false;
        bool m_overdraw = false;
    };

    class HammerRenderViewportWidget : public AtomToolsFramework::RenderViewportWidget
    {
    public:
        using RenderViewportWidget::RenderViewportWidget;

        AzFramework::WindowSize GetClientAreaSize() const override;
        AzFramework::WindowSize GetRenderResolution() const override;
    };

    class HammerWidget
        : public QWidget
        , private AzToolsFramework::EditorEntityContextNotificationBus::Handler
    {
    Q_OBJECT
    public:
        explicit HammerWidget(QWidget* parent);
        ~HammerWidget() override;

        static HammerWidget* CreateAdopting(QWidget* parent, QWidget& realViewport);

        void SetActive(bool active);
        void SetRenderTickEnabled(bool enabled);

        void SetViewModes(const HammerViewModes& viewModes);
        HammerViewModes GetViewModes() const;

        AZ::EntityId GetCameraEntityId() const;

    Q_SIGNALS:
        void ViewportFocusRequested();

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct AdoptTag
        {
        };
        HammerWidget(QWidget* parent, QWidget& realViewport, AdoptTag);

        void InitializeSceneIfReady();
        void InitializeScene();
        void ApplyActiveState();
        void ApplyRenderTickState();
        void ApplyViewModes();
        void SyncAdoptedGeometry();
        void SetupCamera();

        void OnContextReset() override;

        AtomToolsFramework::RenderViewportWidget* m_viewportWidget = nullptr;
        QWidget* m_adoptedRealViewport = nullptr;
        AZ::EntityId m_cameraEntityId;
        AzFramework::ViewportControllerPtr m_cameraController;
        HammerViewModes m_viewModes;
        bool m_sceneInitialized = false;
        bool m_active = false;
        bool m_renderTickEnabled = true;
        AZ::RPI::ViewportContext::PipelineChangedEvent::Handler m_pipelineChangedHandler;
    };
}
