#pragma once

#if !defined(Q_MOC_RUN)
#include <QPointer>
#include <QWidget>
#include <AzCore/std/containers/vector.h>
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
    class HammerWidget;

    class HammerViewportWidget
        : public QWidget
        , private AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler
        , private HammerViewportRequestBus::Handler
    {
        Q_OBJECT
    public:
        static constexpr int MaxViewportCount = 4;

        HammerViewportWidget(QMainWindow* viewPaneHost, EditorViewportWidget& realViewport);
        ~HammerViewportWidget() override;

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

        QMainWindow* m_dockHost = nullptr;
        AzQtComponents::FancyDocking* m_fancyDocking = nullptr;
        QDockWidget* m_dockAnchor = nullptr;
        QAction* m_addViewportAction = nullptr;
        QPointer<QToolButton> m_addViewportButton;
        AZStd::vector<HammerWidget*> m_viewports;
        AZStd::vector<QPointer<QToolBar>> m_viewportToolBars;
        HammerWidget* m_activeViewport = nullptr;
        HammerWidget* m_adoptedViewport = nullptr;
        HammerWidget* m_preGameModeActiveViewport = nullptr;
        QWidget* m_viewportUiOverlayWindow = nullptr;
        QTimer* m_overlaySyncTimer = nullptr;
        bool m_cameraMirroringEnabled = false;
    };
} // namespace Hammer
