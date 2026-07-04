#include "HammerWidget.h"
#include "HammerQtEnvironment.h"
#include "HammerRenderViewportWidget.h"
#include "HammerViewportCameraFactory.h"
#include "HammerViewportManipulatorController.h"

#include <AzCore/Interface/Interface.h>
#include <Hammer/HammerEditorViewportBus.h>

#include <QEvent>

#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/ViewportControllerList.h>

#include <Tools/ui_HammerWidget.h>

namespace Hammer::Platform
{
    void SyncAdoptedViewportGeometry(QWidget& adoptedRealViewport, const QRect& targetGeometry);
} // namespace Hammer::Platform

namespace Hammer
{
    namespace
    {
        AZ::Name PerViewportContextName(AzFramework::ViewportId viewportId)
        {
            return AZ::Name(AZStd::string::format("Hammer Viewport %u", static_cast<unsigned>(viewportId)));
        }

        void SetOverlayPassEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled)
        {
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);

            AZ::RPI::Ptr<AZ::RPI::Pass> twoDPass;
            pipeline && (twoDPass = pipeline->FindFirstPass(AZ::Name("2DPass")), true);
            AZ_Error(
                "HammerWidget", twoDPass,
                "Could not find this viewport's own \"2DPass\" - entity icons may keep rendering in inactive viewports");
            twoDPass && (twoDPass->SetEnabled(enabled), true);
        }

        void SyncViewportContextName(AZ::RPI::ViewportContextPtr viewportContext, AzFramework::ViewportId viewportId, bool active)
        {
            AZ_Assert(viewportContext, "SyncViewportContextName called with a null ViewportContext");
            AZ_Assert(viewportId != AzFramework::InvalidViewportId, "SyncViewportContextName called with an invalid ViewportId");

            auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
            AZ_Error("HammerWidget", viewportContextManager, "Could not find AZ::RPI::ViewportContextRequestsInterface");

            const AZ::Name defaultName = viewportContextManager->GetDefaultViewportContextName();
            const AZ::Name currentName = viewportContext->GetName();

            const AZStd::array<AZ::Name, 2> candidateNames = { PerViewportContextName(viewportId), defaultName };
            const AZ::Name& desiredName = candidateNames[static_cast<size_t>(active)];

            (currentName != desiredName) && (viewportContextManager->RenameViewportContext(viewportContext, desiredName), true);
        }

        void AddToRenderTick(AZ::RPI::RenderPipeline& pipeline)
        {
            pipeline.AddToRenderTick();
        }

        void RemoveFromRenderTick(AZ::RPI::RenderPipeline& pipeline)
        {
            pipeline.RemoveFromRenderTick();
        }

        using RenderTickAction = void (*)(AZ::RPI::RenderPipeline&);
        constexpr AZStd::array<RenderTickAction, 2> RenderTickActions = { &RemoveFromRenderTick, &AddToRenderTick };

        void SetPipelineRenderTickEnabled(AZ::RPI::ViewportContextPtr viewportContext, bool enabled)
        {
            AZ::RPI::RenderPipelinePtr pipeline;
            viewportContext && (pipeline = viewportContext->GetCurrentPipeline(), true);
            pipeline && (RenderTickActions[static_cast<size_t>(enabled)](*pipeline), true);
        }
    } // namespace

    HammerWidget::HammerWidget(QWidget* parent)
        : QWidget(parent)
        , m_ui(new Ui::HammerWidgetClass())
    {
        m_ui->setupUi(this);

        m_viewportWidget = new HammerRenderViewportWidget(this, false);
        AZ_Assert(m_viewportWidget, "Failed to allocate the Hammer render viewport widget");
        m_viewportWidget->installEventFilter(this);

        m_ui->mainLayout->addWidget(m_viewportWidget);
    }

    HammerWidget::~HammerWidget()
    {
        m_adoptedRealViewport && (m_adoptedRealViewport->setParent(nullptr), true);

        const bool neverInitialized = !m_sceneInitialized && m_viewportWidget;
        AZ_Warning(
            "HammerWidget", !neverInitialized || AZ::RHI::RHISystemInterface::Get(),
            "A Hammer viewport was destroyed before ever being shown, and the RHI system has already shut down; skipping "
            "deferred initialization (a known AtomToolsFramework::RenderViewportWidget shutdown issue may occur)");

        (neverInitialized && AZ::RHI::RHISystemInterface::Get()) && (m_viewportWidget->InitializeViewportContext(), true);
    }

    HammerWidget* HammerWidget::CreateAdopting(QWidget* parent, QWidget& realViewport)
    {
        return new HammerWidget(parent, realViewport, AdoptTag{});
    }

    HammerWidget::HammerWidget(QWidget* parent, QWidget& realViewport, AdoptTag)
        : QWidget(parent)
    {
        m_adoptedRealViewport = &realViewport;
        realViewport.hide();
        realViewport.setParent(nullptr);
        realViewport.setParent(this);

        m_viewportWidget = FindRealRenderViewport(&realViewport);
        AZ_Error("HammerWidget", m_viewportWidget, "Could not resolve the adopted real viewport's inner RenderViewportWidget");
        m_viewportWidget && (m_viewportWidget->installEventFilter(this), true);

        SyncAdoptedGeometry();

        m_sceneInitialized = true;

        ApplyActiveState();
        ApplyRenderTickState();
    }

    // Forces both the adopted widget and its inner render surface to match our own size,
    // rather than trusting the adopted widget's own internal layout to cascade the resize
    // down to its render surface on its own.
    void HammerWidget::SyncAdoptedGeometry()
    {
        m_adoptedRealViewport && (Platform::SyncAdoptedViewportGeometry(*m_adoptedRealViewport, rect()), true);
        m_viewportWidget &&
            (Platform::SyncAdoptedViewportGeometry(*m_viewportWidget, QRect(QPoint(0, 0), rect().size())), true);
    }

    void HammerWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        m_adoptedRealViewport && (SyncAdoptedGeometry(), true);
        InitializeSceneIfReady();
    }

    void HammerWidget::showEvent(QShowEvent* event)
    {
        QWidget::showEvent(event);
        m_adoptedRealViewport && (SyncAdoptedGeometry(), true);
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

        AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext();
        SetOverlayPassEnabled(viewportContext, m_active);
        SyncViewportContextName(viewportContext, m_viewportWidget->GetId(), m_active);
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

        m_viewportWidget->GetControllerList()->Add(CreateViewportCameraController(m_viewportWidget->GetId()));
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>());

        ApplyActiveState();
        ApplyRenderTickState();
    }

    void HammerWidget::ApplyRenderTickState()
    {
        AZ_Assert(m_viewportWidget, "ApplyRenderTickState called without an initialized viewport widget");
        SetPipelineRenderTickEnabled(m_viewportWidget->GetViewportContext(), m_renderTickEnabled);
    }
}
