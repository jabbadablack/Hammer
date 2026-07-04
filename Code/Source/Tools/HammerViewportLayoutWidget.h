#pragma once

#if !defined(Q_MOC_RUN)
#include <QScopedPointer>
#include <QWidget>
#endif

#include "HammerLazyFind.h"

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

class QGridLayout;
class QTimer;

namespace Ui
{
    class HammerViewportLayoutWidgetClass;
}

namespace Hammer
{
    class HammerWidget;
    class ActiveViewportTracker;

    class HammerViewportLayoutWidget
        : public QWidget
        , private AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override;

        void AdoptRealPerspectiveViewport(QWidget& realViewport);
        void SetViewportCount(int count);
        void ToggleMaximizeActiveViewport();

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

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<HammerWidget*> m_viewports;
        AZStd::shared_ptr<ActiveViewportTracker> m_activeViewportTracker;
        HammerWidget* m_activeViewport = nullptr;
        HammerWidget* m_adoptedViewport = nullptr;
        HammerWidget* m_preGameModeActiveViewport = nullptr;
        LazyFind<QWidget> m_viewportUiOverlayWindow;
        QTimer* m_overlaySyncTimer = nullptr;
        QScopedPointer<Ui::HammerViewportLayoutWidgetClass> m_ui;
        AZStd::array<HammerWidget*, MaxViewportCount> m_gridSlotWidget = {};
        AZStd::optional<int> m_maximizedFromIndex;
        int m_preMaximizeViewportCount = MinViewportCount;
        int m_currentViewportCount = MinViewportCount;
    };
} // namespace Hammer
