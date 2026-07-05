#include "HammerViewportLayoutWidget.h"
#include "HammerWidget.h"

#include "HammerQtEnvironment.h"

#include <AzToolsFramework/Viewport/ViewportSettings.h>
#include <Editor/EditorViewportSettings.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Viewport/CameraState.h>

#include <QEvent>
#include <QGridLayout>
#include <QTimer>

#include <Tools/ui_HammerViewportLayoutWidget.h>

namespace Hammer
{
    HammerViewportLayoutWidget::HammerViewportLayoutWidget(QWidget* parent)
        : QWidget(parent)
        , m_ui(new Ui::HammerViewportLayoutWidgetClass())
    {
        AZ_Assert(
            !HammerEditorActiveViewportRequestBus::HasHandlers(),
            "HammerViewportLayoutWidget constructed while another HammerEditorActiveViewportRequestBus handler exists");
        HammerEditorActiveViewportRequestBus::Handler::BusConnect();

        m_ui->setupUi(this);
        m_gridContainer = m_ui->gridContainer;
        m_gridLayout = m_ui->gridLayout;

        for (int i = 0; i < MaxViewportCount; ++i)
        {
            HammerWidget* viewport = new HammerWidget(m_gridContainer);
            AZ_Assert(viewport, "Failed to allocate HammerWidget #%d", i);
            m_viewports.push_back(viewport);
        }

        for (HammerWidget* viewport : m_viewports)
        {
            connect(
                viewport, &HammerWidget::ViewportFocusRequested, this,
                [this, viewport]
                {
                    ActivateViewport(viewport);
                });
        }

        SetViewportCount(1);

        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusConnect();
        Camera::EditorCameraRequestBus::Handler::BusConnect();
    }

    HammerViewportLayoutWidget::~HammerViewportLayoutWidget()
    {
        Camera::EditorCameraRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorLegacyGameModeNotificationBus::Handler::BusDisconnect();
        HammerEditorActiveViewportRequestBus::Handler::BusDisconnect();
    }

    void HammerViewportLayoutWidget::SetActiveViewportId(AzFramework::ViewportId viewportId)
    {
        m_activeViewportId = viewportId;
    }

    AzFramework::ViewportId HammerViewportLayoutWidget::GetActiveViewportId() const
    {
        return m_activeViewportId;
    }

    void HammerViewportLayoutWidget::OnStartGameModeRequest()
    {
        m_adoptedViewport && (m_preGameModeActiveViewport = m_activeViewport, ActivateViewport(m_adoptedViewport), true);
    }

    void HammerViewportLayoutWidget::OnStopGameModeRequest()
    {
        m_preGameModeActiveViewport && (ActivateViewport(m_preGameModeActiveViewport), true);
        m_preGameModeActiveViewport = nullptr;
    }

    void HammerViewportLayoutWidget::ActivateViewport(HammerWidget* viewport)
    {
        AZ_Assert(viewport, "ActivateViewport called with a null viewport");
        AZ_Assert(!m_viewports.empty(), "ActivateViewport called with no viewports");

        const bool alreadyActive = viewport == m_activeViewport;

        (!alreadyActive && m_activeViewport) && (m_activeViewport->SetActive(false), true);
        !alreadyActive &&
            (viewport->SetActive(true), m_activeViewport = viewport,
             AzToolsFramework::SetIconsVisible(viewport == m_adoptedViewport),
             Camera::EditorCameraNotificationBus::Broadcast(
                 &Camera::EditorCameraNotificationBus::Events::OnViewportViewEntityChanged, viewport->GetCameraEntityId()),
             true);
    }

    void HammerViewportLayoutWidget::ReconcileGridSlots(int shownCount, int columns)
    {
        AZ_Assert(
            shownCount >= 0 && shownCount <= MaxViewportCount, "ReconcileGridSlots given an out-of-range shownCount %d", shownCount);
        AZ_Assert(columns == 1 || columns == 2, "ReconcileGridSlots given an unexpected column count %d", columns);

        m_adoptedViewportHiddenBehindMaximize &&
            (m_gridLayout->removeWidget(m_adoptedViewport), m_adoptedViewportHiddenBehindMaximize = false, true);

        AZStd::array<HammerWidget*, MaxViewportCount> desiredSlotWidget = {};
        for (int i = 0; i < shownCount; ++i)
        {
            desiredSlotWidget[i] = m_viewports[i];
        }

        for (HammerWidget* viewport : m_viewports)
        {
            const auto oldIt = AZStd::find(m_gridSlotWidget.begin(), m_gridSlotWidget.end(), viewport);
            const auto newIt = AZStd::find(desiredSlotWidget.begin(), desiredSlotWidget.end(), viewport);

            const bool wasShown = oldIt != m_gridSlotWidget.end();
            const bool willShow = newIt != desiredSlotWidget.end();
            const int oldSlot = static_cast<int>(AZStd::distance(m_gridSlotWidget.begin(), oldIt));
            const int newSlot = static_cast<int>(AZStd::distance(desiredSlotWidget.begin(), newIt));
            const bool unchanged = wasShown && willShow && (oldSlot == newSlot);
            const bool hidingAdoptedWhileMaximized =
                wasShown && !willShow && viewport == m_adoptedViewport && m_maximizedFromIndex != -1;

            (wasShown && !unchanged) && (m_gridLayout->removeWidget(viewport), true);
            (wasShown && !willShow && !hidingAdoptedWhileMaximized) &&
                (viewport->hide(), viewport->SetRenderTickEnabled(false), true);
            hidingAdoptedWhileMaximized &&
                (m_gridLayout->addWidget(viewport, 0, 0), viewport->lower(), viewport->show(),
                 viewport->SetRenderTickEnabled(true), m_adoptedViewportHiddenBehindMaximize = true, true);
            (willShow && !unchanged) &&
                (m_gridLayout->addWidget(viewport, newSlot / columns, newSlot % columns), viewport->show(),
                 viewport->SetRenderTickEnabled(true), true);
        }

        m_gridSlotWidget = desiredSlotWidget;
    }

    void HammerViewportLayoutWidget::SetViewportCount(int count)
    {
        AZ_Assert(m_gridLayout, "SetViewportCount called before the grid layout was constructed");
        AZ_Assert(m_viewports.size() == MaxViewportCount, "SetViewportCount expects all %d viewports to already exist", MaxViewportCount);

        count = AZStd::clamp(count, MinViewportCount, MaxViewportCount);

        (count != 1) && (RestoreMaximizeSwap(), true);

        const int columns = 1 + static_cast<int>(count > 1);
        const int shownCount = AZStd::GetMin(count, static_cast<int>(m_viewports.size()));

        ReconcileGridSlots(shownCount, columns);

        ActivateViewport(m_viewports[0]);

        m_currentViewportCount = count;
        emit ViewportCountChanged(count);
    }

    void HammerViewportLayoutWidget::AdoptRealPerspectiveViewport(QWidget& realViewport)
    {
        AZ_Assert(!m_adoptedViewport, "AdoptRealPerspectiveViewport called more than once");
        AZ_Assert(
            m_viewports.size() == MaxViewportCount, "AdoptRealPerspectiveViewport expects all %d viewports to already exist",
            MaxViewportCount);
        AZ_Assert(m_activeViewport == m_viewports[0], "AdoptRealPerspectiveViewport expects slot 0 to still be the active viewport");

        HammerWidget* placeholder = m_viewports[0];

        HammerWidget* adopted = HammerWidget::CreateAdopting(m_gridContainer, realViewport);
        AZ_Assert(adopted, "Failed to allocate the adopting HammerWidget");

        connect(
            adopted, &HammerWidget::ViewportFocusRequested, this,
            [this, adopted]
            {
                ActivateViewport(adopted);
            });

        m_viewports[0] = adopted;
        m_adoptedViewport = adopted;

        const auto slotIt = AZStd::find(m_gridSlotWidget.begin(), m_gridSlotWidget.end(), placeholder);
        (slotIt != m_gridSlotWidget.end()) && (*slotIt = adopted, true);

        placeholder->SetRenderTickEnabled(false);

        m_gridLayout->addWidget(adopted, 0, 0);
        adopted->show();
        realViewport.show();
        adopted->SetRenderTickEnabled(true);

        ActivateViewport(adopted);

        delete placeholder;

        ResolveViewportUiOverlayWindow();
    }

    void HammerViewportLayoutWidget::ResolveViewportUiOverlayWindow()
    {
        AZ_Assert(m_adoptedViewport, "ResolveViewportUiOverlayWindow called before the real viewport was adopted");

        m_viewportUiOverlayWindow || (m_viewportUiOverlayWindow = FindViewportUiOverlayWindow(m_adoptedViewport), true);
        m_viewportUiOverlayWindow && (m_viewportUiOverlayWindow->installEventFilter(this), true);

        AZ_Assert(!m_overlaySyncTimer, "ResolveViewportUiOverlayWindow should only start the sync timer once");
        m_overlaySyncTimer = new QTimer(this);
        connect(m_overlaySyncTimer, &QTimer::timeout, this, &HammerViewportLayoutWidget::SyncViewportUiOverlay);
        m_overlaySyncTimer->start(16);
    }

    void HammerViewportLayoutWidget::SyncViewportUiOverlay()
    {
        const bool shouldChase = m_viewportUiOverlayWindow && m_activeViewport && m_activeViewport != m_adoptedViewport;

        shouldChase &&
            (m_viewportUiOverlayWindow->setGeometry(QRect(m_activeViewport->mapToGlobal(QPoint(0, 0)), m_activeViewport->size())),
             true);
    }

    bool HammerViewportLayoutWidget::eventFilter(QObject* watched, QEvent* event)
    {
        const bool isOverlayMoveOrResize = watched == m_viewportUiOverlayWindow && m_activeViewport &&
            m_activeViewport != m_adoptedViewport && (event->type() == QEvent::Move || event->type() == QEvent::Resize);

        isOverlayMoveOrResize &&
            (static_cast<QWidget*>(watched)->setGeometry(
                 QRect(m_activeViewport->mapToGlobal(QPoint(0, 0)), m_activeViewport->size())),
             true);

        return QWidget::eventFilter(watched, event);
    }

    void HammerViewportLayoutWidget::RestoreMaximizeSwap()
    {
        AZ_Assert(
            m_maximizedFromIndex == -1 || m_maximizedFromIndex < static_cast<int>(m_viewports.size()),
            "RestoreMaximizeSwap has an out-of-range maximized index %d", m_maximizedFromIndex);
        (m_maximizedFromIndex != -1) && (AZStd::swap(m_viewports[0], m_viewports[m_maximizedFromIndex]), true);
        m_maximizedFromIndex = -1;
    }

    void HammerViewportLayoutWidget::MaximizeActiveViewport()
    {
        AZ_Assert(!m_viewports.empty(), "MaximizeActiveViewport called with no viewports to maximize");
        AZ_Assert(m_maximizedFromIndex == -1, "MaximizeActiveViewport called while already maximized");
        AZ_Assert(m_activeViewport, "MaximizeActiveViewport called before any viewport was ever activated");

        const auto it = AZStd::find(m_viewports.begin(), m_viewports.end(), m_activeViewport);
        const int activeIndex = it != m_viewports.end() ? static_cast<int>(AZStd::distance(m_viewports.begin(), it)) : 0;

        m_preMaximizeViewportCount = m_currentViewportCount;
        m_maximizedFromIndex = activeIndex;
        AZStd::swap(m_viewports[0], m_viewports[activeIndex]);

        SetViewportCount(1);
    }

    void HammerViewportLayoutWidget::RestoreFromMaximize()
    {
        AZ_Assert(m_maximizedFromIndex != -1, "RestoreFromMaximize called while not maximized");
        AZ_Assert(
            m_preMaximizeViewportCount >= MinViewportCount && m_preMaximizeViewportCount <= MaxViewportCount,
            "RestoreFromMaximize has an out-of-range pre-maximize viewport count %d", m_preMaximizeViewportCount);

        const int restoreCount = m_preMaximizeViewportCount;
        RestoreMaximizeSwap();
        SetViewportCount(restoreCount);
    }

    void HammerViewportLayoutWidget::ToggleMaximizeActiveViewport()
    {
        AZ_Assert(!m_viewports.empty(), "ToggleMaximizeActiveViewport called with no viewports");

        using Action = void (HammerViewportLayoutWidget::*)();
        static constexpr AZStd::array<Action, 2> Actions = {
            &HammerViewportLayoutWidget::MaximizeActiveViewport, &HammerViewportLayoutWidget::RestoreFromMaximize
        };
        (this->*Actions[static_cast<size_t>(m_maximizedFromIndex != -1)])();
    }

    void HammerViewportLayoutWidget::SetViewFromEntityPerspective(const AZ::EntityId& entityId)
    {
        const auto it = AZStd::find_if(
            m_viewports.begin(), m_viewports.end(),
            [&entityId](HammerWidget* viewport)
            {
                return viewport->GetCameraEntityId() == entityId;
            });
        (it != m_viewports.end()) && (ActivateViewport(*it), true);
    }

    AZ::EntityId HammerViewportLayoutWidget::GetCurrentViewEntityId()
    {
        return m_activeViewport ? m_activeViewport->GetCameraEntityId() : AZ::EntityId();
    }

    bool HammerViewportLayoutWidget::GetActiveCameraPosition(AZ::Vector3& cameraPos)
    {
        const bool hasActiveCamera = m_activeViewport && m_activeViewport->GetCameraEntityId().IsValid();
        hasActiveCamera &&
            (AZ::TransformBus::EventResult(
                 cameraPos, m_activeViewport->GetCameraEntityId(), &AZ::TransformBus::Events::GetWorldTranslation),
             true);
        return hasActiveCamera;
    }

    AZStd::optional<AZ::Transform> HammerViewportLayoutWidget::GetActiveCameraTransform()
    {
        const bool hasActiveCamera = m_activeViewport && m_activeViewport->GetCameraEntityId().IsValid();
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        hasActiveCamera &&
            (AZ::TransformBus::EventResult(transform, m_activeViewport->GetCameraEntityId(), &AZ::TransformBus::Events::GetWorldTM),
             true);
        return hasActiveCamera ? AZStd::optional<AZ::Transform>(transform) : AZStd::nullopt;
    }

    AZStd::optional<float> HammerViewportLayoutWidget::GetCameraFoV()
    {
        return SandboxEditor::CameraDefaultFovRadians();
    }

    bool HammerViewportLayoutWidget::GetActiveCameraState(AzFramework::CameraState& cameraState)
    {
        const bool hasActiveCamera = m_activeViewport && m_activeViewport->GetCameraEntityId().IsValid();
        AZ::Transform transform = AZ::Transform::CreateIdentity();
        hasActiveCamera &&
            (AZ::TransformBus::EventResult(transform, m_activeViewport->GetCameraEntityId(), &AZ::TransformBus::Events::GetWorldTM),
             true);
        hasActiveCamera && (cameraState = AzFramework::CreateDefaultCamera(transform, AzFramework::ScreenSize(1, 1)), true);
        return hasActiveCamera;
    }
} // namespace Hammer
