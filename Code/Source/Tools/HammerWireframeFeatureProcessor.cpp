#include "HammerWireframeFeatureProcessor.h"

#include <Atom/RHI.Reflect/AttachmentEnums.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Pass/PassTemplate.h>
#include <Atom/RPI.Reflect/Pass/RasterPassData.h>
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
            const Data::Asset<RPI::MaterialAsset> materialAsset = RPI::AssetUtils::LoadCriticalAsset<RPI::MaterialAsset>(path);
            AZ_Warning("HammerWireframeFeatureProcessor", materialAsset.GetId().IsValid(), "Failed to load material asset at '%s'", path.c_str());

            const Data::Instance<RPI::Material> material = RPI::Material::FindOrCreate(materialAsset);
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

        // Expected transiently during early engine startup, before the editor entity context
        // exists yet; log once rather than every periodic scan so this isn't spam during that
        // window (this function also runs once per second via OnTick).
        static bool loggedNullContext = false;
        if (editorContextId.IsNull())
        {
            AZ_Warning("HammerWireframeFeatureProcessor", loggedNullContext, "Editor entity context not available yet; skipping entity scan");
            loggedNullContext = true;
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
        // Expected transiently at startup while the wireframe material asset is still loading
        // (see OnTick); log once rather than every frame so this isn't spam during that window.
        static bool loggedMissingMaterial = false;
        if (!m_material)
        {
            AZ_Warning("HammerWireframeFeatureProcessor", loggedMissingMaterial, "Wireframe material not yet loaded; skipping render");
            loggedMissingMaterial = true;
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

    AzFramework::NativeWindowHandle HammerWireframeFeatureProcessor::s_wireframeWindowHandle = nullptr;

    void HammerWireframeFeatureProcessor::SetWireframeWindow(AzFramework::NativeWindowHandle windowHandle)
    {
        s_wireframeWindowHandle = windowHandle;
    }

    // Appends a wireframe RasterPass onto an already-built default pipeline, reading (not
    // clearing) its existing color/depth output, so wireframe draws on top of normal rendering
    // instead of needing its own standalone pipeline. Modeled on
    // EditorModeFeedback/Code/Source/Pass/EditorStatePassSystem.cpp::AddPassesToRenderPipeline.
    void HammerWireframeFeatureProcessor::AddRenderPasses(RPI::RenderPipeline* renderPipeline)
    {
        if (renderPipeline->GetWindowHandle() != s_wireframeWindowHandle)
        {
            return;
        }

        const Name passName("HammerWireframeOverlay");
        if (renderPipeline->FindFirstPass(passName))
        {
            return;
        }

        const Name postProcessPassName("PostProcessPass");
        const Name depthPrePassName("DepthPrePass");
        if (!renderPipeline->FindFirstPass(postProcessPassName) || !renderPipeline->FindFirstPass(depthPrePassName))
        {
            AZ_Warning(
                "HammerWireframeFeatureProcessor", false,
                "PostProcessPass/DepthPrePass not found in render pipeline; wireframe overlay disabled");
            return;
        }

        const Name templateName("HammerWireframeOverlayTemplate");
        auto passTemplate = RPI::PassSystemInterface::Get()->GetPassTemplate(templateName);
        if (!passTemplate)
        {
            auto newTemplate = AZStd::make_shared<RPI::PassTemplate>();
            newTemplate->m_name = templateName;
            newTemplate->m_passClass = Name("RasterPass");

            RPI::PassSlot colorSlot;
            colorSlot.m_name = Name("ColorInputOutput");
            colorSlot.m_slotType = RPI::PassSlotType::InputOutput;
            colorSlot.m_scopeAttachmentUsage = RHI::ScopeAttachmentUsage::RenderTarget;
            newTemplate->AddSlot(colorSlot);

            RPI::PassSlot depthSlot;
            depthSlot.m_name = Name("InputDepth");
            depthSlot.m_slotType = RPI::PassSlotType::Input;
            depthSlot.m_scopeAttachmentUsage = RHI::ScopeAttachmentUsage::DepthStencil;
            newTemplate->AddSlot(depthSlot);

            auto passData = AZStd::make_shared<RPI::RasterPassData>();
            passData->m_drawListTag = Name("hammerwireframe");
            passData->m_pipelineViewTag = Name("MainCamera");
            passData->m_bindViewSrg = true;
            newTemplate->m_passData = passData;

            RPI::PassSystemInterface::Get()->AddPassTemplate(templateName, newTemplate);
            passTemplate = newTemplate;
        }

        RPI::PassRequest passRequest;
        passRequest.m_passName = passName;
        passRequest.m_templateName = templateName;
        passRequest.AddInputConnection(
            RPI::PassConnection{ Name("ColorInputOutput"), RPI::PassAttachmentRef{ postProcessPassName, Name("Output") } });
        passRequest.AddInputConnection(
            RPI::PassConnection{ Name("InputDepth"), RPI::PassAttachmentRef{ depthPrePassName, Name("Depth") } });

        RPI::Ptr<RPI::Pass> pass = RPI::PassSystemInterface::Get()->CreatePassFromRequest(&passRequest);
        AZ_Error("HammerWireframeFeatureProcessor", pass, "Failed to create the wireframe overlay pass from request");
        if (!pass)
        {
            return;
        }

        AZ_Error(
            "HammerWireframeFeatureProcessor", renderPipeline->AddPassAfter(pass, postProcessPassName),
            "Failed to add the wireframe overlay pass to the render pipeline");
    }
} // namespace Hammer
