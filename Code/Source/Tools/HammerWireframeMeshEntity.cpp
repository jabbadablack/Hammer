#include "HammerWireframeMeshEntity.h"

#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <AzCore/Component/TransformBus.h>

namespace Hammer
{
    using namespace AZ;

    HammerWireframeMeshEntity::HammerWireframeMeshEntity(EntityId entityId, Data::Instance<RPI::Material> material, RPI::Scene* scene)
        : m_entityId(entityId)
        , m_scene(scene)
        , m_material(material)
    {
        // Safe here: the constructor only ever runs from main-thread contexts (Activate(),
        // OnEditorEntityCreated, or the periodic OnTick rescan).
        TransformBus::EventResult(m_cachedWorldTM, m_entityId, &TransformBus::Events::GetWorldTM);

        TransformNotificationBus::Handler::BusConnect(m_entityId);
        Render::MeshHandleStateNotificationBus::Handler::BusConnect(m_entityId);
    }

    HammerWireframeMeshEntity::~HammerWireframeMeshEntity()
    {
        Render::MeshHandleStateNotificationBus::Handler::BusDisconnect();
        TransformNotificationBus::Handler::BusDisconnect();
    }

    void HammerWireframeMeshEntity::OnTransformChanged(const Transform&, const Transform& worldTM)
    {
        m_cachedWorldTM = worldTM;
    }

    bool HammerWireframeMeshEntity::CanDraw() const
    {
        return !m_drawPackets.empty();
    }

    void HammerWireframeMeshEntity::RetryIfNotYetDrawable()
    {
        if (CanDraw() || !m_meshHandle || !m_meshHandle->IsValid() || !m_scene)
        {
            return;
        }

        auto* featureProcessor = m_scene->GetFeatureProcessor<Render::MeshFeatureProcessorInterface>();
        if (!featureProcessor)
        {
            return;
        }

        if (const auto model = featureProcessor->GetModel(*m_meshHandle))
        {
            CreateOrUpdateDrawPackets(featureProcessor, model);
        }
    }

    void HammerWireframeMeshEntity::ClearDrawData()
    {
        m_drawPackets.clear();
    }

    void HammerWireframeMeshEntity::Draw()
    {
        if (!CanDraw() || !m_meshHandle || !m_meshHandle->IsValid() || !m_scene)
        {
            return;
        }

        RPI::DynamicDrawInterface* dynamicDraw = RPI::GetDynamicDraw();
        for (auto& drawPacket : m_drawPackets)
        {
            drawPacket.Update(*m_scene);
            if (const auto* rhiDrawPacket = drawPacket.GetRHIDrawPacket())
            {
                dynamicDraw->AddDrawPacket(m_scene, rhiDrawPacket);
            }
        }
    }

    void HammerWireframeMeshEntity::OnMeshHandleSet(const Render::MeshFeatureProcessorInterface::MeshHandle* meshHandle)
    {
        m_meshHandle = meshHandle;
        if (!m_meshHandle || !m_meshHandle->IsValid())
        {
            ClearDrawData();
            return;
        }

        if (!m_scene)
        {
            return;
        }

        auto* featureProcessor = m_scene->GetFeatureProcessor<Render::MeshFeatureProcessorInterface>();
        if (!featureProcessor)
        {
            return;
        }

        if (const auto model = featureProcessor->GetModel(*m_meshHandle))
        {
            CreateOrUpdateDrawPackets(featureProcessor, model);
        }
    }

    void HammerWireframeMeshEntity::CreateOrUpdateDrawPackets(
        const Render::MeshFeatureProcessorInterface* featureProcessor, Data::Instance<RPI::Model> model)
    {
        ClearDrawData();

        if (!m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        const auto objectSrg = CreateObjectSrg(featureProcessor);
        BuildDrawPackets(model->GetModelAsset(), objectSrg);
    }

    void HammerWireframeMeshEntity::BuildDrawPackets(Data::Asset<RPI::ModelAsset> modelAsset, Data::Instance<RPI::ShaderResourceGroup> objectSrg)
    {
        const auto modelLodAssets = modelAsset->GetLodAssets();
        const Data::Asset<RPI::ModelLodAsset>& modelLodAsset = modelLodAssets[0];
        Data::Instance<RPI::ModelLod> modelLod = RPI::ModelLod::FindOrCreate(modelLodAsset, modelAsset).get();

        for (size_t i = 0; i < modelLod->GetMeshes().size(); i++)
        {
            m_drawPackets.emplace_back(HammerWireframeDrawPacket(*modelLod, i, m_material, objectSrg));
        }
    }

    Data::Instance<RPI::ShaderResourceGroup> HammerWireframeMeshEntity::CreateObjectSrg(
        const Render::MeshFeatureProcessorInterface* featureProcessor) const
    {
        const auto& shaderAsset = m_material->GetAsset()->GetMaterialTypeAsset()->GetShaderAssetForObjectSrg();
        const auto& objectSrgLayout = m_material->GetAsset()->GetObjectSrgLayout();
        const auto objectSrg = RPI::ShaderResourceGroup::Create(shaderAsset, objectSrgLayout->GetName());

        const auto objectId = featureProcessor->GetObjectId(*m_meshHandle).GetIndex();
        RHI::ShaderInputNameIndex objectIdIndex = "m_objectId";
        objectSrg->SetConstant(objectIdIndex, objectId);
        objectSrg->Compile();

        return objectSrg;
    }
} // namespace Hammer
