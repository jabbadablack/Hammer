#pragma once

#if !defined(Q_MOC_RUN)
#include <QScopedPointer>
#include <QWidget>
#include <Atom/RPI.Public/ViewportContext.h>
#include <AzCore/Component/EntityId.h>
#include <AzFramework/Viewport/ViewportControllerInterface.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#endif

namespace AtomToolsFramework
{
    class RenderViewportWidget;
}

namespace Ui
{
    class HammerWidgetClass;
}

class QResizeEvent;
class QShowEvent;
class QEvent;
class QObject;

namespace Hammer
{
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
        void SyncAdoptedGeometry();
        void SetupCamera();

        void OnContextReset() override;

        AtomToolsFramework::RenderViewportWidget* m_viewportWidget = nullptr;
        QWidget* m_adoptedRealViewport = nullptr;
        AZ::EntityId m_cameraEntityId;
        AzFramework::ViewportControllerPtr m_cameraController;
        QScopedPointer<Ui::HammerWidgetClass> m_ui;
        bool m_sceneInitialized = false;
        bool m_active = false;
        bool m_renderTickEnabled = true;
        AZ::RPI::ViewportContext::PipelineChangedEvent::Handler m_pipelineChangedHandler;
    };
}
