#include "HammerWidget.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>

namespace Hammer
{
    HammerWidget::HammerWidget(QWidget* parent)
        : QWidget(parent)
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        auto* viewport1 = new AtomToolsFramework::RenderViewportWidget(this);
        viewport1->InitializeViewportContext();

        if (auto* sceneSystem = AzFramework::SceneSystemInterface::Get())
        {
            if (AZStd::shared_ptr<AzFramework::Scene> mainScene = sceneSystem->GetScene(AzFramework::Scene::MainSceneName))
            {
                viewport1->SetScene(mainScene);
            }
        }

        mainLayout->addWidget(viewport1);
        setLayout(mainLayout);
    }
}
