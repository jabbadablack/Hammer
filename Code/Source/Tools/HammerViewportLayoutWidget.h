
#pragma once

#if !defined(Q_MOC_RUN)
#include <QWidget>
#endif

#include <AzCore/std/containers/vector.h>

class QGridLayout;
class QPushButton;

namespace Hammer
{
    class HammerWidget;

    // Hosts 1-4 viewports arranged in a grid, with a small toolbar to switch the count. Every slot
    // is always a HammerWidget instance (each with its own independent camera controller, matching
    // the default Editor viewport's feel); the real Editor viewport is never shown in the grid -
    // see SetHiddenRealViewport(). Selection and gizmos come from HammerViewportManipulatorController,
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
        // here, permanently hidden - it's never shown in any grid slot, all of which always use
        // HammerWidget. Some Editor/AzToolsFramework systems (the single-address
        // Camera::EditorCameraRequestBus/CameraNotificationBus handlers, and m_pPrimaryViewport)
        // are only ever implemented by a real EditorViewportWidget instance, and other Editor
        // tooling breaks if none exists anywhere - keeping one alive (just never displayed) is
        // meant to satisfy that without needing to actually show it.
        void SetHiddenRealViewport(QWidget* realViewport);

    private:
        void SetViewportCount(int count);

        QGridLayout* m_gridLayout = nullptr;
        QWidget* m_gridContainer = nullptr;
        AZStd::vector<QPushButton*> m_countButtons;
        AZStd::vector<HammerWidget*> m_viewports;
        QWidget* m_hiddenRealViewport = nullptr;
    };
} // namespace Hammer
