#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/API/EditorCameraBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <Hammer/HammerEditorViewportBus.h>
#endif

class QGridLayout;
class QTimer;

namespace Hammer
{
    class HammerWidget;

    class HammerViewportLayoutWidget
        : public QWidget
        , private AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler
        , private Camera::EditorCameraRequestBus::Handler
        , private HammerEditorActiveViewportRequestBus::Handler
        , private HammerViewportRequestBus::Handler
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override;

        void AdoptRealPerspectiveViewport(QWidget& realViewport);
        void SetViewportCount(int count) override;
        void ToggleMaximizeActiveViewport() override;

    Q_SIGNALS:
        void ViewportCountChanged(int count);

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        void RestoreMaximizeSwap();
        void MaximizeActiveViewport();
        void RestoreFromMaximize();
        void ActivateViewport(HammerWidget* viewport);
        void ReconcileGridSlots(int shownCount, int columns);
        void ResolveViewportUiOverlayWindow();
        void SyncViewportUiOverlay();

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

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<HammerWidget*> m_viewports;
        AzFramework::ViewportId m_activeViewportId = AzFramework::InvalidViewportId;
        HammerWidget* m_activeViewport = nullptr;
        HammerWidget* m_adoptedViewport = nullptr;
        HammerWidget* m_preGameModeActiveViewport = nullptr;
        QWidget* m_viewportUiOverlayWindow = nullptr;
        QTimer* m_overlaySyncTimer = nullptr;
        AZStd::array<HammerWidget*, MaxViewportCount> m_gridSlotWidget = {};
        int m_maximizedFromIndex = -1;
        int m_preMaximizeViewportCount = MinViewportCount;
        int m_currentViewportCount = MinViewportCount;
        bool m_adoptedViewportHiddenBehindMaximize = false;
    };
} // namespace Hammer
