#include "HammerWireframeMeshEntity.h"

#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/Model/ModelLodUtils.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <AzCore/Component/TransformBus.h>

namespace Hammer
{
    using namespace AZ;

    namespace
    {
        // Utility class for common drawable data, matching the pattern used by EditorModeFeedback's DrawableMeshEntity.
        class DrawableMetaData
        {
        public:
            explicit DrawableMetaData(EntityId entityId)
            {
                m_scene = RPI::Scene::GetSceneForEntityId(entityId);
                if (m_scene)
                {
                    const auto viewportContextRequests = RPI::ViewportContextRequests::Get();
                    if (const auto viewportContext = viewportContextRequests->GetViewportContextByScene(m_scene))
                    {
                        m_view = viewportContext->GetDefaultView();
                    }
                    m_featureProcessor = m_scene->GetFeatureProcessor<Render::MeshFeatureProcessorInterface>();
                }
            }

            RPI::Scene* m_scene = nullptr;
            RPI::ViewPtr m_view;
            Render::MeshFeatureProcessorInterface* m_featureProcessor = nullptr;
        };
    } // namespace

    HammerWireframeMeshEntity::HammerWireframeMeshEntity(EntityId entityId, Data::Instance<RPI::Material> material)
        : m_entityId(entityId)
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
        if (CanDraw() || !m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        const DrawableMetaData metaData(m_entityId);
        if (!metaData.m_featureProcessor)
        {
            return;
        }

        if (const auto model = metaData.m_featureProcessor->GetModel(*m_meshHandle))
        {
            const auto modelLodIndex = GetModelLodIndex(metaData.m_view, model);
            CreateOrUpdateDrawPackets(metaData.m_featureProcessor, modelLodIndex, model);
        }
    }

    void HammerWireframeMeshEntity::ClearDrawData()
    {
        m_modelLodIndex = RPI::ModelLodIndex::Null;
        m_drawPackets.clear();
    }

    void HammerWireframeMeshEntity::Draw()
    {
        if (!CanDraw() || !m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        const DrawableMetaData metaData(m_entityId);
        if (!metaData.m_scene || !metaData.m_featureProcessor)
        {
            return;
        }

        const auto model = metaData.m_featureProcessor->GetModel(*m_meshHandle);
        if (!model)
        {
            return;
        }

        if (const auto modelLodIndex = GetModelLodIndex(metaData.m_view, model); m_modelLodIndex != modelLodIndex)
        {
            CreateOrUpdateDrawPackets(metaData.m_featureProcessor, modelLodIndex, model);
        }

        RPI::DynamicDrawInterface* dynamicDraw = RPI::GetDynamicDraw();
        for (auto& drawPacket : m_drawPackets)
        {
            drawPacket.Update(*metaData.m_scene);
            if (const auto* rhiDrawPacket = drawPacket.GetRHIDrawPacket())
            {
                dynamicDraw->AddDrawPacket(metaData.m_scene, rhiDrawPacket);
            }
        }
    }

    RPI::ModelLodIndex HammerWireframeMeshEntity::GetModelLodIndex(const RPI::ViewPtr view, Data::Instance<RPI::Model> model) const
    {
        return RPI::ModelLodUtils::SelectLod(view.get(), m_cachedWorldTM, *model);
    }

    void HammerWireframeMeshEntity::OnMeshHandleSet(const Render::MeshFeatureProcessorInterface::MeshHandle* meshHandle)
    {
        m_meshHandle = meshHandle;
        if (!m_meshHandle || !m_meshHandle->IsValid())
        {
            AZ_Warning("HammerWireframeMeshEntity", false, "OnMeshHandleSet: invalid handle for entity %s", m_entityId.ToString().c_str());
            ClearDrawData();
            return;
        }

        const DrawableMetaData metaData(m_entityId);
        if (!metaData.m_featureProcessor)
        {
            AZ_Warning("HammerWireframeMeshEntity", false, "OnMeshHandleSet: no MeshFeatureProcessorInterface for entity %s", m_entityId.ToString().c_str());
            return;
        }

        if (const auto model = metaData.m_featureProcessor->GetModel(*m_meshHandle))
        {
            const auto modelLodIndex = GetModelLodIndex(metaData.m_view, model);
            CreateOrUpdateDrawPackets(metaData.m_featureProcessor, modelLodIndex, model);
            const auto worldAabb = model->GetModelAsset()->GetAabb().GetTransformedAabb(m_cachedWorldTM);
            AZ_Warning(
                "HammerWireframeMeshEntity", false,
                "OnMeshHandleSet: entity %s built %zu draw packets, worldPos=(%.2f,%.2f,%.2f), worldAabbMin=(%.2f,%.2f,%.2f) worldAabbMax=(%.2f,%.2f,%.2f)",
                m_entityId.ToString().c_str(), m_drawPackets.size(),
                double(m_cachedWorldTM.GetTranslation().GetX()), double(m_cachedWorldTM.GetTranslation().GetY()), double(m_cachedWorldTM.GetTranslation().GetZ()),
                double(worldAabb.GetMin().GetX()), double(worldAabb.GetMin().GetY()), double(worldAabb.GetMin().GetZ()),
                double(worldAabb.GetMax().GetX()), double(worldAabb.GetMax().GetY()), double(worldAabb.GetMax().GetZ()));
        }
        else
        {
            AZ_Warning("HammerWireframeMeshEntity", false, "OnMeshHandleSet: GetModel() returned null for entity %s", m_entityId.ToString().c_str());
        }
    }

    void HammerWireframeMeshEntity::CreateOrUpdateDrawPackets(
        const Render::MeshFeatureProcessorInterface* featureProcessor, const RPI::ModelLodIndex modelLodIndex, Data::Instance<RPI::Model> model)
    {
        ClearDrawData();

        if (!m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        const auto objectSrg = CreateObjectSrg(featureProcessor);
        m_modelLodIndex = modelLodIndex;
        BuildDrawPackets(model->GetModelAsset(), objectSrg);
    }

    void HammerWireframeMeshEntity::BuildDrawPackets(Data::Asset<RPI::ModelAsset> modelAsset, Data::Instance<RPI::ShaderResourceGroup> objectSrg)
    {
        const auto modelLodAssets = modelAsset->GetLodAssets();
        const Data::Asset<RPI::ModelLodAsset>& modelLodAsset = modelLodAssets[m_modelLodIndex.m_index];
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
