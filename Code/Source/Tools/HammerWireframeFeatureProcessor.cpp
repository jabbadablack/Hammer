#include "HammerWireframeFeatureProcessor.h"

#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Entity/EntityContextBus.h>

namespace Hammer
{
    using namespace AZ;

    namespace
    {
        Data::Instance<RPI::Material> CreateWireframeMaterial()
        {
            const AZStd::string path = "materials/hammerwireframe.azmaterial";
            const auto materialAsset = RPI::AssetUtils::LoadCriticalAsset<RPI::MaterialAsset>(path);
            AZ_Warning("HammerWireframeFeatureProcessor", materialAsset.GetId().IsValid(), "Failed to load material asset at '%s'", path.c_str());

            const auto material = RPI::Material::FindOrCreate(materialAsset);
            AZ_Warning("HammerWireframeFeatureProcessor", material, "Material::FindOrCreate returned null for '%s'", path.c_str());

            return material;
        }
    } // namespace

    void HammerWireframeFeatureProcessor::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<HammerWireframeFeatureProcessor, RPI::FeatureProcessor>()->Version(1);
        }
    }

    void HammerWireframeFeatureProcessor::Activate()
    {
        ScanForNewEntities();

        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();
        TickBus::Handler::BusConnect();

        AZ_Warning("HammerWireframeFeatureProcessor", false, "Activated: tracking %zu entities", m_meshEntities.size());
    }

    void HammerWireframeFeatureProcessor::ScanForNewEntities()
    {
        AzFramework::EntityContextId editorContextId = AzFramework::EntityContextId::CreateNull();
        AzToolsFramework::EditorEntityContextRequestBus::BroadcastResult(
            editorContextId, &AzToolsFramework::EditorEntityContextRequests::GetEditorEntityContextId);

        if (editorContextId.IsNull())
        {
            return;
        }

        ComponentApplicationBus::Broadcast(
            &ComponentApplicationRequests::EnumerateEntities,
            [this, editorContextId](Entity* entity)
            {
                const EntityId entityId = entity->GetId();
                if (m_meshEntities.find(entityId) != m_meshEntities.end())
                {
                    return;
                }

                AzFramework::EntityContextId owningContextId = AzFramework::EntityContextId::CreateNull();
                AzFramework::EntityIdContextQueryBus::EventResult(
                    owningContextId, entityId, &AzFramework::EntityIdContextQueries::GetOwningContextId);

                if (owningContextId == editorContextId)
                {
                    TrackEntity(entityId);
                }
            });
    }

    void HammerWireframeFeatureProcessor::Deactivate()
    {
        TickBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusDisconnect();
        m_meshEntities.clear();
    }

    void HammerWireframeFeatureProcessor::TrackEntity(EntityId entityId)
    {
        m_meshEntities.emplace(entityId, AZStd::make_unique<HammerWireframeMeshEntity>(entityId, m_material, GetParentScene()));
    }

    void HammerWireframeFeatureProcessor::OnEditorEntityCreated(const EntityId& entityId)
    {
        TrackEntity(entityId);
    }

    void HammerWireframeFeatureProcessor::OnEditorEntityDeleted(const EntityId& entityId)
    {
        m_meshEntities.erase(entityId);
    }

    void HammerWireframeFeatureProcessor::PruneDeadEntities()
    {
        // Undo/redo and prefab template reloads can destroy and recreate entities without
        // reliably firing OnEditorEntityDeleted for the old instance, which would otherwise
        // leak a stale tracked entry (and its GPU-side draw packet/SRG resources) forever.
        AZStd::vector<EntityId> deadEntityIds;
        for (const auto& [entityId, meshEntity] : m_meshEntities)
        {
            Entity* entity = nullptr;
            ComponentApplicationBus::BroadcastResult(entity, &ComponentApplicationRequests::FindEntity, entityId);
            if (!entity)
            {
                deadEntityIds.push_back(entityId);
            }
        }

        for (const EntityId& entityId : deadEntityIds)
        {
            m_meshEntities.erase(entityId);
        }

        if (!deadEntityIds.empty())
        {
            AZ_Warning(
                "HammerWireframeFeatureProcessor", false, "PruneDeadEntities: removed %zu stale entries, now tracking %zu entities",
                deadEntityIds.size(), m_meshEntities.size());
        }
    }

    void HammerWireframeFeatureProcessor::OnTick(float deltaTime, ScriptTimePoint)
    {
        if (!m_material)
        {
            m_material = CreateWireframeMaterial();
        }

        constexpr float ScanIntervalSeconds = 1.0f;
        m_entityScanTimer += deltaTime;
        if (m_entityScanTimer >= ScanIntervalSeconds)
        {
            m_entityScanTimer = 0.0f;
            PruneDeadEntities();

            const size_t beforeCount = m_meshEntities.size();
            ScanForNewEntities();
            if (m_meshEntities.size() != beforeCount)
            {
                AZ_Warning(
                    "HammerWireframeFeatureProcessor", false, "ScanForNewEntities: now tracking %zu entities (was %zu)",
                    m_meshEntities.size(), beforeCount);
            }

            for (auto& [entityId, meshEntity] : m_meshEntities)
            {
                meshEntity->RetryIfNotYetDrawable();
            }
        }
    }

    void HammerWireframeFeatureProcessor::Render(const RenderPacket&)
    {
        if (!m_material)
        {
            return;
        }

        static size_t lastLoggedDrawableCount = SIZE_MAX;
        size_t drawableCount = 0;
        for (auto& [entityId, meshEntity] : m_meshEntities)
        {
            if (meshEntity->CanDraw())
            {
                ++drawableCount;
            }
        }
        if (drawableCount != lastLoggedDrawableCount)
        {
            lastLoggedDrawableCount = drawableCount;
            AZ_Warning(
                "HammerWireframeFeatureProcessor", false, "Render(): %zu tracked entities, %zu drawable",
                m_meshEntities.size(), drawableCount);
        }

        for (auto& [entityId, meshEntity] : m_meshEntities)
        {
            if (meshEntity->CanDraw())
            {
                meshEntity->Draw();
            }
        }
    }
} // namespace Hammer
