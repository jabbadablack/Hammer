#pragma once

#if !defined(Q_MOC_RUN)
#include <QPointer>
#include <QWidget>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/EditorCameraBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <Hammer/HammerEditorViewportBus.h>
#endif

class QAction;
class QDockWidget;
class QMainWindow;
class QTimer;
class QToolBar;
class QToolButton;

namespace AzQtComponents
{
    class FancyDocking;
} // namespace AzQtComponents

namespace Hammer
{
    class HammerWidget;

    class HammerViewportWidget
        : public QWidget
        , private AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler
        , private Camera::EditorCameraRequestBus::Handler
        , private HammerEditorActiveViewportRequestBus::Handler
        , private HammerViewportRequestBus::Handler
    {
        Q_OBJECT
    public:
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportWidget(QWidget* parent = nullptr);
        ~HammerViewportWidget() override;

        void AdoptRealPerspectiveViewport(QWidget& realViewport);
        void SetActiveViewportViewModes(bool normal, bool wireframe, bool overdraw) override;
        void SetCameraMirroringEnabled(bool enabled) override;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void ActivateViewport(HammerWidget* viewport);
        void ResolveViewportUiOverlayWindow();
        void SyncViewportUiOverlay();
        QToolBar* BuildViewportToolBar(size_t viewportIndex);

        void OnStartGameModeRequest() override;
        void OnStopGameModeRequest() override;

        void SetActiveViewportId(AzFramework::ViewportId viewportId) override;
        AzFramework::ViewportId GetActiveViewportId() const override;

        void SetViewFromEntityPerspective(const AZ::EntityId& entityId) override;
        AZ::EntityId GetCurrentViewEntityId() override;
        bool GetActiveCameraPosition(AZ::Vector3& cameraPos) override;
        AZStd::optional<AZ::Transform> GetActiveCameraTransform() override;
        AZStd::optional<float> GetCameraFoV() override;
        bool GetActiveCameraState(AzFramework::CameraState& cameraState) override;

        QMainWindow* m_dockHost = nullptr;
        AzQtComponents::FancyDocking* m_fancyDocking = nullptr;
        QDockWidget* m_dockAnchor = nullptr;
        QAction* m_addViewportAction = nullptr;
        QPointer<QToolButton> m_addViewportButton;
        AZStd::vector<HammerWidget*> m_viewports;
        AZStd::vector<QPointer<QToolBar>> m_viewportToolBars;
        AzFramework::ViewportId m_activeViewportId = AzFramework::InvalidViewportId;
        HammerWidget* m_activeViewport = nullptr;
        HammerWidget* m_adoptedViewport = nullptr;
        HammerWidget* m_preGameModeActiveViewport = nullptr;
        QWidget* m_viewportUiOverlayWindow = nullptr;
        QTimer* m_overlaySyncTimer = nullptr;
        bool m_cameraMirroringEnabled = false;
    };
} // namespace Hammer
