#pragma once

#if !defined(Q_MOC_RUN)
#include <QPointer>
#include <QWidget>
#include <Atom/RPI.Public/ViewportContext.h>
#include <AzCore/std/containers/array.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <Hammer/HammerEditorViewportBus.h>
#endif

class EditorViewportWidget;
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
    struct ViewModes
    {
        bool m_normal = true;
        bool m_wireframe = false;
        bool m_overdraw = false;
    };

    class ViewportLayout
        : public QWidget
        , private AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler
        , private ViewportRequestBus::Handler
    {
        Q_OBJECT
    public:
        static constexpr int MaxViewportCount = 4;

        ViewportLayout(QMainWindow* viewPaneHost, EditorViewportWidget& realViewport);
        ~ViewportLayout() override;

        void SetCameraMirroringEnabled(bool enabled) override;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        struct Slot
        {
            EditorViewportWidget* m_engineViewport = nullptr;
            QDockWidget* m_dock = nullptr;
            ViewModes m_viewModes;
            QPointer<QToolBar> m_toolBar;
            AZ::RPI::ViewportContext::PipelineChangedEvent::Handler m_pipelineChanged;
            AZ::RPI::ViewportContext::SizeChangedEvent::Handler m_sizeChanged;
        };

        void InitializeSlot(int index, EditorViewportWidget* engineViewport);
        void ActivateViewport(Slot& slot);
        void ApplyViewModes(Slot& slot);
        void SyncViewportUiOverlay();
        QToolBar* BuildViewportToolBar(size_t viewportIndex);

        void OnStartGameModeRequest() override;
        void OnStopGameModeRequest() override;

        QMainWindow* m_dockHost = nullptr;
        AzQtComponents::FancyDocking* m_fancyDocking = nullptr;
        QDockWidget* m_dockAnchor = nullptr;
        QAction* m_addViewportAction = nullptr;
        QPointer<QToolButton> m_addViewportButton;
        AZStd::array<Slot, MaxViewportCount> m_slots;
        Slot* m_activeSlot = nullptr;
        Slot* m_preGameModeActiveSlot = nullptr;
        QWidget* m_viewportUiOverlayWindow = nullptr;
        QTimer* m_overlaySyncTimer = nullptr;
        bool m_cameraMirroringEnabled = false;
        bool m_gameModeSuppressed = false;
    };
} // namespace Hammer
