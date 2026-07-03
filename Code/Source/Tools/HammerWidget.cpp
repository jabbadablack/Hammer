#include "HammerWidget.h"
#include "HammerViewportManipulatorController.h"

#include <Hammer/IHammerRenderBackend.h>

#include <AzCore/Interface/Interface.h>
#include <Hammer/HammerEditorViewportBus.h>

#include <QEvent>
#include <QVBoxLayout>

#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/ViewportControllerList.h>

namespace Hammer
{
    HammerWidget::HammerWidget(QWidget* parent)
        : QWidget(parent)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        auto* renderBackend = AZ::Interface<IHammerRenderBackend>::Get();
        AZ_Assert(renderBackend, "IHammerRenderBackend must be registered before constructing a HammerWidget");
        m_viewportWidget = renderBackend->CreateRenderViewportWidget(this);
        AZ_Assert(m_viewportWidget, "Failed to allocate the Hammer render viewport widget");
        m_viewportWidget->installEventFilter(this);

        mainLayout->addWidget(m_viewportWidget);
        setLayout(mainLayout);
    }

    void HammerWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        InitializeSceneIfReady();
    }

    void HammerWidget::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        InitializeSceneIfReady();
    }

    bool HammerWidget::eventFilter(QObject* watched, QEvent* event)
    {
        (watched == m_viewportWidget && event->type() == QEvent::FocusIn) && (emit ViewportFocusRequested(), true);
        return QWidget::eventFilter(watched, event);
    }

    void HammerWidget::SetActive(bool active)
    {
        m_active = active;
        (m_sceneInitialized && m_viewportWidget) && (ApplyActiveState(), true);
    }

    void HammerWidget::SetRenderTickEnabled(bool enabled)
    {
        m_renderTickEnabled = enabled;
        (m_sceneInitialized && m_viewportWidget) && (ApplyRenderTickState(), true);
    }

    void HammerWidget::ApplyActiveState()
    {
        AZ_Assert(m_viewportWidget, "ApplyActiveState called without an initialized viewport widget");
        AZ_Assert(m_sceneInitialized, "ApplyActiveState called before the scene was initialized");

        m_viewportWidget->SetInputProcessingEnabled(m_active);

        m_active &&
            (HammerEditorActiveViewportRequestBus::Broadcast(
                 &HammerEditorActiveViewportRequests::SetActiveViewportId, m_viewportWidget->GetId()),
             true);

        auto* renderBackend = AZ::Interface<IHammerRenderBackend>::Get();
        AZ_Assert(renderBackend, "IHammerRenderBackend must be registered before ApplyActiveState is called");

        AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext();
        renderBackend->SetOverlayPassEnabled(viewportContext, m_active);
        renderBackend->SyncViewportContextName(viewportContext, m_viewportWidget->GetId(), m_active);
    }

    void HammerWidget::InitializeSceneIfReady()
    {
        const bool shouldInitialize =
            !m_sceneInitialized && m_viewportWidget && m_viewportWidget->width() >= 1 && m_viewportWidget->height() >= 1;

        shouldInitialize && (InitializeScene(), true);
    }

    void HammerWidget::InitializeScene()
    {
        AZ_Assert(!m_sceneInitialized, "InitializeScene called after the scene was already initialized");
        AZ_Assert(m_viewportWidget, "InitializeScene called without an allocated viewport widget");

        m_viewportWidget->InitializeViewportContext();
        m_sceneInitialized = true;

        AzFramework::ISceneSystem* sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Error("HammerWidget", sceneSystem, "AzFramework::SceneSystemInterface is not available; Hammer cannot bind a scene");

        AZStd::shared_ptr<AzFramework::Scene> mainScene;
        sceneSystem && (mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName), true);
        AZ_Error("HammerWidget", mainScene, "Main scene '%s' not found", AzFramework::Scene::MainSceneName.data());
        mainScene && (m_viewportWidget->SetScene(mainScene, /*useDefaultRenderPipeline*/ true), true);

        auto* renderBackend = AZ::Interface<IHammerRenderBackend>::Get();
        AZ_Assert(renderBackend, "IHammerRenderBackend must be registered before InitializeSceneIfReady is called");
        m_viewportWidget->GetControllerList()->Add(renderBackend->BuildViewportCameraController(m_viewportWidget->GetId()));
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>());

        ApplyActiveState();
        ApplyRenderTickState();
    }

    void HammerWidget::ApplyRenderTickState()
    {
        AZ_Assert(m_viewportWidget, "ApplyRenderTickState called without an initialized viewport widget");
        auto* renderBackend = AZ::Interface<IHammerRenderBackend>::Get();
        AZ_Assert(renderBackend, "IHammerRenderBackend must be registered before ApplyRenderTickState is called");
        renderBackend->SetRenderTickEnabled(m_viewportWidget->GetViewportContext(), m_renderTickEnabled);
    }
}
