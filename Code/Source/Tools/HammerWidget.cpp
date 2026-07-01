#include "HammerWidget.h"
#include "HammerViewportCameraFactory.h"
#include "HammerViewportManipulatorController.h"
#include "HammerWireframeFeatureProcessor.h"
#include <QVBoxLayout>
#include <Atom/RPI.Public/ViewportContext.h>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/ViewportControllerList.h>

namespace Hammer
{
    HammerWidget::HammerWidget(bool wireframe, AtomToolsFramework::RenderViewportWidget* cameraSource, QWidget* parent)
        : QWidget(parent)
        , m_cameraSource(cameraSource)
        , m_wireframe(wireframe)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        AtomToolsFramework::RenderViewportWidget* viewport1 = new AtomToolsFramework::RenderViewportWidget(this);
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

        AZ_Warning(
            "HammerWidget", false, "InitializeSceneIfReady: initializing wireframe=%d size=%dx%d visible=%d", m_wireframe,
            m_viewportWidget->width(), m_viewportWidget->height(), m_viewportWidget->isVisible());

        if (m_wireframe)
        {
            // Must be registered before SetScene below, since it triggers the default pipeline's
            // AddRenderPasses() synchronously, which is where the wireframe overlay pass gets added.
            m_wireframeWindowHandle = reinterpret_cast<AzFramework::NativeWindowHandle>(m_viewportWidget->winId());
            HammerWireframeFeatureProcessor::AddWireframeWindow(m_wireframeWindowHandle);
        }

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

        if (m_cameraSource)
        {
            // No camera input of our own: this viewport just follows m_cameraSource's camera
            // transform, since the default viewport camera (installed on m_cameraSource) already
            // handles navigation.
            AZ::RPI::ViewportContextPtr sourceContext = m_cameraSource->GetViewportContext();
            AZ::RPI::ViewportContextPtr targetContext = m_viewportWidget->GetViewportContext();
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
        }
        else
        {
            m_viewportWidget->GetControllerList()->Add(CreateViewportCameraController(m_viewportWidget->GetId()));
        }

        m_viewportWidget->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>());
    }

    HammerWidget::~HammerWidget()
    {
        if (m_wireframeWindowHandle)
        {
            HammerWireframeFeatureProcessor::RemoveWireframeWindow(m_wireframeWindowHandle);
        }
    }
}
