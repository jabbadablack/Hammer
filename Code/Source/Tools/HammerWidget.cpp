#include "HammerWidget.h"
#include "HammerViewportCameraFactory.h"
#include "HammerViewportManipulatorController.h"
#include <QEvent>
#include <QVBoxLayout>
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

    HammerWidget::HammerWidget(QWidget* parent)
        : QWidget(parent)
    {
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

        // AtomViewportDisplayInfoSystemComponent (the FPS/resolution/render-pipeline debug text
        // overlay, Gems/AtomLyIntegration/AtomViewportDisplayInfo) - and potentially other
        // Atom/Editor systems - unconditionally read AZ::RPI::ViewportContextRequestsInterface::
        // GetDefaultViewportContext() rather than whichever viewport they're actually drawing into.
        // HammerViewportLayoutWidget::SetHiddenRealViewport() already frees the "default"
        // ViewportContext name/designation from the real (permanently hidden, no-longer-resized)
        // Editor viewport once, at startup, so it's available to claim here. Claiming it for
        // whichever Hammer viewport is actually active - and releasing it again on deactivation, so
        // the next-activated viewport can claim it in turn - keeps that reference pointed at
        // something live and correctly sized. AZ::RPI::ViewportContextManager::RenameViewportContext()
        // asserts and silently no-ops if the target name is already claimed by another context, so
        // both branches below check the current name first to avoid redundant/erroneous calls.
        if (auto* viewportContextManager = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get())
        {
            AZ::RPI::ViewportContextPtr viewportContext = m_viewportWidget->GetViewportContext();
            const AZ::Name defaultName = viewportContextManager->GetDefaultViewportContextName();
            if (m_active)
            {
                if (viewportContext->GetName() != defaultName)
                {
                    viewportContextManager->RenameViewportContext(viewportContext, defaultName);
                }
            }
            else if (viewportContext->GetName() == defaultName)
            {
                viewportContextManager->RenameViewportContext(
                    viewportContext, AZ::Name(AZStd::string::format("Hammer Viewport %u", static_cast<unsigned>(m_viewportWidget->GetId()))));
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

        // Every viewport gets its own independent camera controller, matching the default Editor
        // viewport's feel.
        m_viewportWidget->GetControllerList()->Add(CreateViewportCameraController(m_viewportWidget->GetId()));
        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>());

        // Applies whatever active state was requested via SetActive() before the controller list
        // existed (e.g. HammerViewportLayoutWidget seeding the initially-active slot at
        // construction time, before any HammerWidget has necessarily finished initializing).
        ApplyActiveState();
    }
}
