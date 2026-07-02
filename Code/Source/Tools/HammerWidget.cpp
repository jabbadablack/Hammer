#include "HammerWidget.h"
#include "HammerActiveViewportTracker.h"
#include "HammerViewportCameraFactory.h"
#include "HammerViewportManipulatorController.h"
#include <QEvent>
#include <QVBoxLayout>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/ViewportControllerList.h>

namespace Hammer
{
    class HammerRenderViewportWidget : public AtomToolsFramework::RenderViewportWidget
    {
    public:
        using RenderViewportWidget::RenderViewportWidget;

        AzFramework::WindowSize GetClientAreaSize() const override
        {
            AzFramework::WindowSize size = RenderViewportWidget::GetClientAreaSize();
            size.m_width = AZStd::max(size.m_width, 32u);
            size.m_height = AZStd::max(size.m_height, 32u);
            return size;
        }

        AzFramework::WindowSize GetRenderResolution() const override
        {
            AzFramework::WindowSize size = RenderViewportWidget::GetRenderResolution();
            size.m_width = AZStd::max(size.m_width, 32u);
            size.m_height = AZStd::max(size.m_height, 32u);
            return size;
        }
    };

    HammerWidget::HammerWidget(QWidget* parent, AZStd::shared_ptr<ActiveViewportTracker> activeViewportTracker)
        : QWidget(parent)
        , m_activeViewportTracker(AZStd::move(activeViewportTracker))
    {
        AZ_Assert(m_activeViewportTracker, "HammerWidget constructed with a null ActiveViewportTracker");

        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        HammerRenderViewportWidget* viewport1 = new HammerRenderViewportWidget(this, false);
        AZ_Assert(viewport1, "Failed to allocate HammerRenderViewportWidget");
        m_viewportWidget = viewport1;
        m_viewportWidget->installEventFilter(this);

        mainLayout->addWidget(viewport1);
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
        if (watched == m_viewportWidget && event->type() == QEvent::FocusIn)
        {
            emit ViewportFocusRequested();
        }
        return QWidget::eventFilter(watched, event);
    }

    void HammerWidget::SetActive(bool active)
    {
        m_active = active;
        if (m_sceneInitialized && m_viewportWidget)
        {
            ApplyActiveState();
        }
    }

    void HammerWidget::ApplyActiveState()
    {
        m_viewportWidget->SetInputProcessingEnabled(m_active);

        if (m_active)
        {
            m_activeViewportTracker->SetActiveViewportId(m_viewportWidget->GetId());
        }

        if (AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext())
        {
            if (AZ::RPI::RenderPipelinePtr pipeline = viewportContext->GetCurrentPipeline())
            {
                AZ::RPI::Ptr<AZ::RPI::Pass> twoDPass = pipeline->FindFirstPass(AZ::Name("2DPass"));
                AZ_Error(
                    "HammerWidget", twoDPass,
                    "Could not find this viewport's own \"2DPass\" - entity icons may keep rendering in inactive viewports");
                if (twoDPass)
                {
                    twoDPass->SetEnabled(m_active);
                }
            }
        }

        if (auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get())
        {
            AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext();
            const AZ::Name defaultName = viewportContextManager->GetDefaultViewportContextName();
            const AZ::Name currentName = viewportContext->GetName();
            const AZ::Name desiredName = m_active
                ? defaultName
                : AZ::Name(AZStd::string::format("Hammer Viewport %u", static_cast<unsigned>(m_viewportWidget->GetId())));
            const bool shouldRename = m_active ? currentName != defaultName : currentName == defaultName;
            if (shouldRename)
            {
                viewportContextManager->RenameViewportContext(viewportContext, desiredName);
            }
        }
    }

    void HammerWidget::InitializeSceneIfReady()
    {
        if (m_sceneInitialized || !m_viewportWidget || m_viewportWidget->width() < 1 || m_viewportWidget->height() < 1)
        {
            return;
        }

        m_viewportWidget->InitializeViewportContext();
        m_sceneInitialized = true;

        AzFramework::ISceneSystem* sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Error("HammerWidget", sceneSystem, "AzFramework::SceneSystemInterface is not available; Hammer cannot bind a scene");
        if (sceneSystem)
        {
            AZStd::shared_ptr<AzFramework::Scene> mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName);
            AZ_Error("HammerWidget", mainScene, "Main scene '%s' not found", AzFramework::Scene::MainSceneName.data());
            if (mainScene)
            {
                m_viewportWidget->SetScene(mainScene, /*useDefaultRenderPipeline*/ true);
            }
        }

        m_viewportWidget->GetControllerList()->Add(CreateViewportCameraController(m_viewportWidget->GetId()));
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>(m_activeViewportTracker));

        ApplyActiveState();
    }
}
