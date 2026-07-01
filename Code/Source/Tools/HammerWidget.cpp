#include "HammerWidget.h"
#include "HammerViewportCameraFactory.h"
#include "HammerViewportManipulatorController.h"
#include "HammerWireframeFeatureProcessor.h"
#include <QVBoxLayout>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/ViewportControllerList.h>

namespace Hammer
{
    HammerWidget::HammerWidget(bool wireframe, QWidget* parent)
        : QWidget(parent)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        AtomToolsFramework::RenderViewportWidget* viewport1 = new AtomToolsFramework::RenderViewportWidget(this);
        viewport1->InitializeViewportContext();

        if (wireframe)
        {
            // Must be registered before SetScene below, since it triggers the default pipeline's
            // AddRenderPasses() synchronously, which is where the wireframe overlay pass gets added.
            HammerWireframeFeatureProcessor::SetWireframeWindow(reinterpret_cast<AzFramework::NativeWindowHandle>(viewport1->winId()));
        }

        AzFramework::ISceneSystem* sceneSystem = AzFramework::SceneSystemInterface::Get();
        AZ_Error("HammerWidget", sceneSystem, "AzFramework::SceneSystemInterface is not available; Hammer cannot bind a scene");
        if (sceneSystem)
        {
            AZStd::shared_ptr<AzFramework::Scene> mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName);
            AZ_Error("HammerWidget", mainScene, "Main scene '%s' not found", AzFramework::Scene::MainSceneName.data());
            if (mainScene)
            {
                viewport1->SetScene(mainScene, /*useDefaultRenderPipeline*/ true);
            }
        }

        viewport1->GetControllerList()->Add(CreateViewportCameraController(viewport1->GetId()));
        viewport1->GetControllerList()->Add(AZStd::make_shared<HammerViewportManipulatorController>());

        mainLayout->addWidget(viewport1);
        setLayout(mainLayout);
    }
}
