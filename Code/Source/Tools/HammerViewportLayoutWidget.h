
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/containers/vector.h>
#include <Atom/RPI.Public/Base.h>

class QEvent;
class QGridLayout;
class QPushButton;
class QTimer;

namespace Hammer
{
    class HammerWidget;

    // Hosts 1-4 viewports arranged in a grid, with a small toolbar to switch the count. Every slot
    // is always a HammerWidget instance (each with its own independent camera controller, matching
    // the default Editor viewport's feel); the real Editor viewport is never shown in any grid slot
    // - see SetHiddenRealViewport(). Selection and gizmos come from HammerViewportManipulatorController,
    // which uses only public AzToolsFramework APIs.
    class HammerViewportLayoutWidget
        : public QWidget
    {
        Q_OBJECT
    public:
        static constexpr int MinViewportCount = 1;
        static constexpr int MaxViewportCount = 4;

        explicit HammerViewportLayoutWidget(QWidget* parent = nullptr);
        ~HammerViewportLayoutWidget() override = default;

        // Keeps realViewport (the real, externally-owned EditorViewportWidget) alive and parented
        // here - it's never shown in any grid slot, all of which always use HammerWidget. It is,
        // however, kept genuinely Qt-visible (see the definition for why) - positioned off-screen,
        // out of the user's way entirely, rather than overlapping any Hammer viewport (an earlier
        // attempt that overlapped it directly over the active viewport caused it to intermittently
        // steal mouse/keyboard input and disrupt camera navigation, even with
        // WA_TransparentForMouseEvents/NoFocus set). Its ViewportUi overlay (a separate,
        // independently-positioned window - transform-mode/reference-space/component-mode widgets,
        // the only reason this real viewport needs to run at all) is forced back on-screen, tracking
        // whichever Hammer viewport is active, via an eventFilter() on that overlay window - see
        // SyncRealViewportToActive() and eventFilter() for the full mechanism. Some Editor/
        // AzToolsFramework systems (the single-address Camera::EditorCameraRequestBus/
        // CameraNotificationBus handlers, and m_pPrimaryViewport) are also only ever implemented by
        // a real EditorViewportWidget instance, and other Editor tooling breaks if none exists
        // anywhere - keeping one alive satisfies that too.
        void SetHiddenRealViewport(QWidget* realViewport);

    private:
        void SetViewportCount(int count);

        // Keeps m_hiddenRealViewport positioned off-screen but sized to match m_activeViewport
        // (so its ViewportUi overlay computes the correct size), syncs its camera transform to
        // match m_activeViewport's, and finds+caches the overlay window the first time it exists so
        // eventFilter() can start correcting its position. See the class comment on
        // SetHiddenRealViewport() and the definition below for the full rationale.
        void SyncRealViewportToActive();

        // Forces m_viewportUiOverlayWindow back to m_activeViewport's on-screen position whenever
        // Qt (via the real viewport's own, private PositionUiOverlayOverRenderViewport() logic)
        // recomputes it from the real viewport's own (off-screen) geometry. See the definition for
        // the recursion guard.
        bool eventFilter(QObject* watched, QEvent* event) override;

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<QPushButton*> m_countButtons;
        AZStd::vector<HammerWidget*> m_viewports;
        QWidget* m_hiddenRealViewport = nullptr;
        HammerWidget* m_activeViewport = nullptr;
        QTimer* m_realViewportSyncTimer = nullptr;
        AZ::RPI::ViewportContextPtr m_hiddenRealViewportContext;
        QWidget* m_viewportUiOverlayWindow = nullptr;
        bool m_realInternalRenderViewportDisabled = false;
    };
} // namespace Hammer
