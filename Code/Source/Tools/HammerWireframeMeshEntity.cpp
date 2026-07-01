#include "HammerWireframeMeshEntity.h"

#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/MathUtils.h>
#include <AzToolsFramework/Entity/EditorEntityInfoBus.h>

namespace Hammer
{
    using namespace AZ;

    namespace
    {
        // Deterministic per-entity color so each mesh's wireframe is visually distinguishable,
        // stable across frames/sessions (a pure function of the entity ID, nothing stored).
        Color ColorForEntity(EntityId entityId)
        {
            const size_t hash = AZStd::hash<EntityId>{}(entityId);
            const float hueRadians = static_cast<float>(hash % 360) * (Constants::TwoPi / 360.0f);
            Color color;
            color.SetFromHSVRadians(hueRadians, 0.75f, 1.0f);
            color.SetA(1.0f);
            return color;
        }
    } // namespace

    HammerWireframeMeshEntity::HammerWireframeMeshEntity(EntityId entityId, Data::Instance<RPI::Material> material, RPI::Scene* scene)
        : m_entityId(entityId)
        , m_scene(scene)
        , m_material(material)
    {
        // Safe here: the constructor only ever runs from main-thread contexts (Activate(),
        // OnEditorEntityCreated, or the periodic OnTick rescan).
        Transform worldTM = Transform::CreateIdentity();
        TransformBus::EventResult(worldTM, m_entityId, &TransformBus::Events::GetWorldTM);
        m_cachedWorldTM = worldTM;

        // IsVisible() reflects the entity's *computed* visibility (own flag combined with
        // ancestor/prefab state), matching what OnEntityVisibilityChanged notifies about.
        AzToolsFramework::EditorEntityInfoRequestBus::EventResult(
            m_isVisible, m_entityId, &AzToolsFramework::EditorEntityInfoRequestBus::Events::IsVisible);

        TransformNotificationBus::Handler::BusConnect(m_entityId);
        Render::MeshComponentNotificationBus::Handler::BusConnect(m_entityId);
        Render::MeshHandleStateNotificationBus::Handler::BusConnect(m_entityId);
        AzToolsFramework::EditorEntityVisibilityNotificationBus::Handler::BusConnect(m_entityId);
    }

    HammerWireframeMeshEntity::~HammerWireframeMeshEntity()
    {
        AzToolsFramework::EditorEntityVisibilityNotificationBus::Handler::BusDisconnect();
        Render::MeshHandleStateNotificationBus::Handler::BusDisconnect();
        Render::MeshComponentNotificationBus::Handler::BusDisconnect();
        TransformNotificationBus::Handler::BusDisconnect();
    }

    void HammerWireframeMeshEntity::OnTransformChanged(const Transform&, const Transform& worldTM)
    {
        m_cachedWorldTM = worldTM;
    }

    void HammerWireframeMeshEntity::OnEntityVisibilityChanged(bool visibility)
    {
        m_isVisible = visibility;
    }

    bool HammerWireframeMeshEntity::CanDraw() const
    {
        return !m_drawPackets.empty() && m_isVisible;
    }

    void HammerWireframeMeshEntity::RetryIfNotYetDrawable()
    {
        // Deliberately not CanDraw(): that also factors in current visibility, and a hidden
        // entity that already has draw packets built shouldn't have them torn down and rebuilt
        // every tick just because it's hidden right now.
        if (!m_drawPackets.empty())
        {
            return;
        }

        // m_scene is set once at construction and never cleared; null here would mean this
        // object outlived (or never had) a valid owning Scene, which should be structurally
        // impossible given how HammerWireframeFeatureProcessor constructs these.
        AZ_Assert(m_scene, "HammerWireframeMeshEntity has no owning Scene for entity %s", m_entityId.ToString().c_str());
        if (!m_scene)
        {
            return;
        }

        // Re-fetch rather than trusting the cached m_meshHandle: this only runs from OnTick
        // (main thread), so it's safe, and it avoids dereferencing a handle that went stale if
        // the owning MeshComponentController was torn down without us catching the transition.
        // A null/invalid handle here is a routine, expected state (no mesh component, or one
        // that hasn't acquired its handle yet), not an error - no diagnostic for that case.
        const Render::MeshFeatureProcessorInterface::MeshHandle* freshHandle = nullptr;
        Render::MeshHandleStateRequestBus::EventResult(freshHandle, m_entityId, &Render::MeshHandleStateRequests::GetMeshHandle);
        m_meshHandle = freshHandle;

        if (!m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        Render::MeshFeatureProcessorInterface* featureProcessor = m_scene->GetFeatureProcessor<Render::MeshFeatureProcessorInterface>();
        AZ_Warning(
            "HammerWireframeMeshEntity", featureProcessor, "No MeshFeatureProcessorInterface registered for the scene; entity %s cannot be drawn",
            m_entityId.ToString().c_str());
        if (!featureProcessor)
        {
            return;
        }

        const Data::Instance<RPI::Model> model = featureProcessor->GetModel(*m_meshHandle);
        if (model)
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
        if (!CanDraw() || !m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        AZ_Assert(m_scene, "HammerWireframeMeshEntity has no owning Scene for entity %s", m_entityId.ToString().c_str());
        if (!m_scene)
        {
            return;
        }

        RPI::DynamicDrawInterface* dynamicDraw = RPI::GetDynamicDraw();
        for (HammerWireframeDrawPacket& drawPacket : m_drawPackets)
        {
            drawPacket.Update(*m_scene);
            const RHI::DrawPacket* rhiDrawPacket = drawPacket.GetRHIDrawPacket();
            if (rhiDrawPacket)
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

        AZ_Assert(m_scene, "HammerWireframeMeshEntity has no owning Scene for entity %s", m_entityId.ToString().c_str());
        if (!m_scene)
        {
            return;
        }

        Render::MeshFeatureProcessorInterface* featureProcessor = m_scene->GetFeatureProcessor<Render::MeshFeatureProcessorInterface>();
        AZ_Warning(
            "HammerWireframeMeshEntity", featureProcessor, "No MeshFeatureProcessorInterface registered for the scene; entity %s cannot be drawn",
            m_entityId.ToString().c_str());
        if (!featureProcessor)
        {
            return;
        }

        const Data::Instance<RPI::Model> model = featureProcessor->GetModel(*m_meshHandle);
        if (model)
        {
            CreateOrUpdateDrawPackets(featureProcessor, model);
        }
    }

    void HammerWireframeMeshEntity::OnModelReady(const Data::Asset<RPI::ModelAsset>&, const Data::Instance<RPI::Model>& model)
    {
        if (!m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        AZ_Assert(m_scene, "HammerWireframeMeshEntity has no owning Scene for entity %s", m_entityId.ToString().c_str());
        if (!m_scene)
        {
            return;
        }

        Render::MeshFeatureProcessorInterface* featureProcessor = m_scene->GetFeatureProcessor<Render::MeshFeatureProcessorInterface>();
        AZ_Warning(
            "HammerWireframeMeshEntity", featureProcessor, "No MeshFeatureProcessorInterface registered for the scene; entity %s cannot be drawn",
            m_entityId.ToString().c_str());
        if (!featureProcessor)
        {
            return;
        }

        CreateOrUpdateDrawPackets(featureProcessor, model);
    }

    void HammerWireframeMeshEntity::OnModelPreDestroy()
    {
        // Drop our reference to the old model's geometry immediately, rather than holding it
        // (and its GPU-side buffers) alive until the next successful rebuild.
        ClearDrawData();
    }

    void HammerWireframeMeshEntity::CreateOrUpdateDrawPackets(
        Render::MeshFeatureProcessorInterface* featureProcessor, const Data::Instance<RPI::Model>& model)
    {
        ClearDrawData();

        if (!m_meshHandle || !m_meshHandle->IsValid())
        {
            return;
        }

        const Data::Instance<RPI::ShaderResourceGroup> objectSrg = CreateObjectSrg(featureProcessor);
        BuildDrawPackets(model->GetModelAsset(), objectSrg);
    }

    void HammerWireframeMeshEntity::BuildDrawPackets(
        const Data::Asset<RPI::ModelAsset>& modelAsset, const Data::Instance<RPI::ShaderResourceGroup>& objectSrg)
    {
        const AZStd::span<const Data::Asset<RPI::ModelLodAsset>> modelLodAssets = modelAsset->GetLodAssets();
        const Data::Asset<RPI::ModelLodAsset>& modelLodAsset = modelLodAssets[0];
        const Data::Instance<RPI::ModelLod> modelLod = RPI::ModelLod::FindOrCreate(modelLodAsset, modelAsset).get();

        const size_t meshCount = modelLod->GetMeshes().size();
        for (size_t i = 0; i < meshCount; i++)
        {
            m_drawPackets.emplace_back(HammerWireframeDrawPacket(*modelLod, i, m_material, objectSrg));
        }
    }

    Data::Instance<RPI::ShaderResourceGroup> HammerWireframeMeshEntity::CreateObjectSrg(
        Render::MeshFeatureProcessorInterface* featureProcessor) const
    {
        const Data::Asset<RPI::ShaderAsset>& shaderAsset = m_material->GetAsset()->GetMaterialTypeAsset()->GetShaderAssetForObjectSrg();
        const RHI::Ptr<RHI::ShaderResourceGroupLayout>& objectSrgLayout = m_material->GetAsset()->GetObjectSrgLayout();
        const Data::Instance<RPI::ShaderResourceGroup> objectSrg = RPI::ShaderResourceGroup::Create(shaderAsset, objectSrgLayout->GetName());

        const uint32_t objectId = featureProcessor->GetObjectId(*m_meshHandle).GetIndex();
        RHI::ShaderInputNameIndex objectIdIndex = "m_objectId";
        objectSrg->SetConstant(objectIdIndex, objectId);

        RHI::ShaderInputNameIndex colorIndex = "m_color";
        objectSrg->SetConstant(colorIndex, ColorForEntity(m_entityId).GetAsVector4());

        objectSrg->Compile();

        return objectSrg;
    }
} // namespace Hammer
