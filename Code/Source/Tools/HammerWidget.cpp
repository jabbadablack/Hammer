#include "HammerWidget.h"
#include "HammerViewportCameraFactory.h"
#include "HammerWireframeFeatureProcessor.h"
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <AtomToolsFramework/Viewport/RenderViewportWidget.h>
#include <AtomToolsFramework/Viewport/ModularViewportCameraControllerRequestBus.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <AzFramework/Viewport/ViewportControllerList.h>
#include <AzToolsFramework/API/EditorCameraBus.h>

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

        mainLayout->addWidget(viewport1);
        setLayout(mainLayout);

        m_viewportId = viewport1->GetId();

        if (wireframe)
        {
            // A floating overlay button rather than a layout item, so it sits on top of the
            // viewport in a corner instead of taking up its own row.
            m_followMainCameraButton = new QPushButton(QStringLiteral("Follow Main Camera"), this);
            m_followMainCameraButton->setFocusPolicy(Qt::NoFocus);
            connect(m_followMainCameraButton, &QPushButton::clicked, this, &HammerWidget::FollowMainViewportCamera);
            m_followMainCameraButton->adjustSize();
            m_followMainCameraButton->raise();
            RepositionFollowCameraButton();
        }
    }

    void HammerWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        RepositionFollowCameraButton();
    }

    void HammerWidget::RepositionFollowCameraButton()
    {
        if (!m_followMainCameraButton)
        {
            return;
        }

        constexpr int margin = 8;
        m_followMainCameraButton->move(width() - m_followMainCameraButton->width() - margin, margin);
    }

    void HammerWidget::FollowMainViewportCamera()
    {
        AZStd::optional<AZ::Transform> mainCameraTransform;
        Camera::EditorCameraRequestBus::BroadcastResult(mainCameraTransform, &Camera::EditorCameraRequests::GetActiveCameraTransform);
        AZ_Warning("HammerWidget", mainCameraTransform.has_value(), "Could not get the main viewport's active camera transform");
        if (!mainCameraTransform.has_value())
        {
            return;
        }

        constexpr float snapDurationSeconds = 0.0f;
        AtomToolsFramework::ModularViewportCameraControllerRequestBus::Event(
            m_viewportId, &AtomToolsFramework::ModularViewportCameraControllerRequests::InterpolateToTransform, *mainCameraTransform,
            snapDurationSeconds);
    }
}
