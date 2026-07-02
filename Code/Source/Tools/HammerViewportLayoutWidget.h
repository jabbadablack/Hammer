
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/containers/vector.h>
#include <Atom/RPI.Public/Base.h>

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
        // however, kept genuinely Qt-visible (see the definition for why) and continuously
        // repositioned/camera-synced to track whichever Hammer viewport is active, so its
        // ViewportUi overlay (transform-mode/reference-space/component-mode widgets - the only
        // things this is actually for) renders correctly without the user perceiving it as a
        // separate viewport. Some Editor/AzToolsFramework systems (the single-address
        // Camera::EditorCameraRequestBus/CameraNotificationBus handlers, and m_pPrimaryViewport)
        // are also only ever implemented by a real EditorViewportWidget instance, and other Editor
        // tooling breaks if none exists anywhere - keeping one alive satisfies that too.
        void SetHiddenRealViewport(QWidget* realViewport);

    private:
        void SetViewportCount(int count);

        // Continuously repositions/resizes m_hiddenRealViewport to exactly cover m_activeViewport's
        // current on-screen rect, and syncs its camera transform to match. See the class comment on
        // SetHiddenRealViewport() and the definition below for the full rationale.
        void SyncRealViewportToActive();

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<QPushButton*> m_countButtons;
        AZStd::vector<HammerWidget*> m_viewports;
        QWidget* m_hiddenRealViewport = nullptr;
        HammerWidget* m_activeViewport = nullptr;
        QTimer* m_realViewportSyncTimer = nullptr;
        AZ::RPI::ViewportContextPtr m_hiddenRealViewportContext;
    };
} // namespace Hammer
