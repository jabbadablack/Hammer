#include "HammerWidget.h"
#include "HammerViewportCameraFactory.h"
#include "HammerViewportManipulatorController.h"
#include <QVBoxLayout>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/ViewportControllerList.h>

namespace Hammer
{
    HammerWidget::HammerWidget(bool isPrimary, QWidget* parent)
        : QWidget(parent)
        , m_isPrimary(isPrimary)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        AtomToolsFramework::RenderViewportWidget* viewport1 = new AtomToolsFramework::RenderViewportWidget(this);
        AZ_Assert(viewport1, "Failed to allocate RenderViewportWidget");
        viewport1->InitializeViewportContext();
        m_viewportWidget = viewport1;

        mainLayout->addWidget(viewport1);
        setLayout(mainLayout);

        InitializeSceneIfReady();
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

    void HammerWidget::InitializeSceneIfReady()
    {
        if (m_sceneInitialized || !m_viewportWidget || m_viewportWidget->width() <= 0 || m_viewportWidget->height() <= 0)
        {
            return;
        }
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

        if (m_isPrimary)
        {
            // Own camera controller, matching the default Editor viewport's feel. Being the first
            // RenderViewportWidget constructed (HammerViewportLayoutWidget always builds slot 0
            // first), this also naturally becomes the default AZ::RPI::ViewportContext that any
            // non-primary HammerWidget instances below will mirror.
            m_viewportWidget->GetControllerList()->Add(CreateViewportCameraController(m_viewportWidget->GetId()));
        }
        else
        {
            // No camera input of our own: mirror the Editor's default viewport camera every time
            // it changes, since the primary Hammer viewport already handles navigation. Looked up
            // by interface (not by widget) so this doesn't need a direct reference to the primary
            // widget.
            auto* viewportContextInterface = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
            AZ_Error("HammerWidget", viewportContextInterface, "AZ::RPI::ViewportContextRequestsInterface is not available");

            AZ::RPI::ViewportContextPtr sourceContext =
                viewportContextInterface ? viewportContextInterface->GetDefaultViewportContext() : nullptr;
            AZ::RPI::ViewportContextPtr targetContext = m_viewportWidget->GetViewportContext();
            AZ_Assert(targetContext, "HammerWidget's own RenderViewportWidget has no ViewportContext after InitializeViewportContext()");

            if (sourceContext && targetContext)
            {
                targetContext->SetCameraTransform(sourceContext->GetCameraTransform());

                m_cameraTransformChangedHandler = AZ::RPI::MatrixChangedEvent::Handler(
                    [sourceContext, targetContext]([[maybe_unused]] const AZ::Matrix4x4& matrix)
                    {
                        targetContext->SetCameraTransform(sourceContext->GetCameraTransform());
                    });
                sourceContext->ConnectViewMatrixChangedHandler(m_cameraTransformChangedHandler);
            }
            else
            {
                // No default viewport context to mirror (unexpected, but recoverable): fall back
                // to giving this viewport its own camera controller so it isn't left unusable.
                AZ_Error(
                    "HammerWidget", false, "No default viewport context to mirror; falling back to an independent camera controller");
                m_viewportWidget->GetControllerList()->Add(CreateViewportCameraController(m_viewportWidget->GetId()));
            }
        }

        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>());
    }
}
