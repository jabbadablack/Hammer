#include "HammerViewModeFeatureProcessor.h"

#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/Model/ModelLod.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Pass/PassRequest.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshComponentBus.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Matrix4x4.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace Hammer
{
    namespace
    {
        constexpr const char* LogWindow = "HammerViewMode";
        constexpr const char* PassTemplatesAssetPath = "Passes/HammerPassTemplates.azasset";
        constexpr const char* MaterialProductPath = "materials/viewmode/hammerviewmode.azmaterial";
        constexpr const char* SrgShaderProductPath = "shaders/viewmode/hammerwireframe.azshader";

        bool g_passTemplatesLoaded = false;
    } // namespace

    void HammerViewModeFeatureProcessor::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        serializeContext &&
            (serializeContext->Class<HammerViewModeFeatureProcessor, AZ::RPI::FeatureProcessor>()->Version(0), true);
    }

    void HammerViewModeFeatureProcessor::Activate()
    {
        auto* passSystem = AZ::RPI::PassSystemInterface::Get();
        AZ_Assert(passSystem, "HammerViewModeFeatureProcessor activated before the pass system");

        (!g_passTemplatesLoaded && passSystem) &&
            (g_passTemplatesLoaded = passSystem->LoadPassTemplateMappings(PassTemplatesAssetPath), true);
        AZ_Error(LogWindow, g_passTemplatesLoaded, "Failed to load '%s'; Hammer view modes are unavailable", PassTemplatesAssetPath);

        QueueAssetLoads();
        EnableSceneNotification();
    }

    void HammerViewModeFeatureProcessor::Deactivate()
    {
        DisableSceneNotification();
        m_meshes.clear();
        m_pipelinePasses.clear();
        m_material = nullptr;
        m_materialAsset.Release();
        m_srgShaderAsset.Release();
    }

    void HammerViewModeFeatureProcessor::QueueAssetLoads()
    {
        const bool needMaterialAsset = !m_materialAsset.GetId().IsValid();
        AZ::Data::AssetId materialAssetId;
        needMaterialAsset &&
            (materialAssetId =
                 AZ::RPI::AssetUtils::GetAssetIdForProductPath(MaterialProductPath, AZ::RPI::AssetUtils::TraceLevel::None),
             true);
        (needMaterialAsset && materialAssetId.IsValid()) &&
            (m_materialAsset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::MaterialAsset>(
                 materialAssetId, AZ::Data::AssetLoadBehavior::PreLoad),
             true);

        const bool needSrgShaderAsset = !m_srgShaderAsset.GetId().IsValid();
        AZ::Data::AssetId srgShaderAssetId;
        needSrgShaderAsset &&
            (srgShaderAssetId =
                 AZ::RPI::AssetUtils::GetAssetIdForProductPath(SrgShaderProductPath, AZ::RPI::AssetUtils::TraceLevel::None),
             true);
        (needSrgShaderAsset && srgShaderAssetId.IsValid()) &&
            (m_srgShaderAsset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::ShaderAsset>(
                 srgShaderAssetId, AZ::Data::AssetLoadBehavior::PreLoad),
             true);
    }

    bool HammerViewModeFeatureProcessor::EnsureAssets()
    {
        QueueAssetLoads();

        (!m_material && m_materialAsset.IsReady()) && (m_material = AZ::RPI::Material::FindOrCreate(m_materialAsset), true);

        return m_material && m_srgShaderAsset.IsReady();
    }

    void HammerViewModeFeatureProcessor::AddRenderPasses(AZ::RPI::RenderPipeline* renderPipeline)
    {
        AZ_Assert(renderPipeline, "AddRenderPasses called with a null render pipeline");

        (!g_passTemplatesLoaded) &&
            (g_passTemplatesLoaded = AZ::RPI::PassSystemInterface::Get()->LoadPassTemplateMappings(PassTemplatesAssetPath), true);

        const bool defaultView = renderPipeline->GetViewType() == AZ::RPI::ViewType::Default;
        auto existingFilter =
            AZ::RPI::PassFilter::CreateWithTemplateName(AZ::Name("HammerWireframePassTemplate"), renderPipeline);
        const bool alreadyInjected = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(existingFilter) != nullptr;
        const bool anchorsPresent = renderPipeline->FindFirstPass(AZ::Name("AuxGeomPass")) != nullptr &&
            renderPipeline->FindFirstPass(AZ::Name("DepthPrePass")) != nullptr;

        AZ_Warning(
            LogWindow, !defaultView || alreadyInjected || anchorsPresent,
            "Pipeline '%s' has no AuxGeomPass/DepthPrePass anchors; Hammer view modes are unavailable in it",
            renderPipeline->GetId().GetCStr());

        (defaultView && !alreadyInjected && anchorsPresent && g_passTemplatesLoaded) && (InjectPasses(*renderPipeline), true);
    }

    void HammerViewModeFeatureProcessor::InjectPasses(AZ::RPI::RenderPipeline& renderPipeline)
    {
        auto* passSystem = AZ::RPI::PassSystemInterface::Get();
        AZ_Assert(passSystem, "InjectPasses called without a pass system");

        AZ::RPI::PassRequest wireframeRequest;
        wireframeRequest.m_passName = AZ::Name("HammerWireframePass");
        wireframeRequest.m_templateName = AZ::Name("HammerWireframePassTemplate");
        wireframeRequest.m_passEnabled = false;
        wireframeRequest.AddInputConnection(AZ::RPI::PassConnection{
            AZ::Name("ColorInputOutput"), AZ::RPI::PassAttachmentRef{ AZ::Name("AuxGeomPass"), AZ::Name("ColorInputOutput") } });
        wireframeRequest.AddInputConnection(AZ::RPI::PassConnection{
            AZ::Name("DepthInputOutput"), AZ::RPI::PassAttachmentRef{ AZ::Name("DepthPrePass"), AZ::Name("Depth") } });
        AZ::RPI::Ptr<AZ::RPI::Pass> wireframePass = passSystem->CreatePassFromRequest(&wireframeRequest);
        wireframePass && (renderPipeline.AddPassAfter(wireframePass, AZ::Name("AuxGeomPass")), true);

        AZ::RPI::PassRequest countRequest;
        countRequest.m_passName = AZ::Name("HammerOverdrawCountPass");
        countRequest.m_templateName = AZ::Name("HammerOverdrawCountPassTemplate");
        countRequest.m_passEnabled = false;
        countRequest.AddInputConnection(AZ::RPI::PassConnection{
            AZ::Name("DepthInputOutput"), AZ::RPI::PassAttachmentRef{ AZ::Name("DepthPrePass"), AZ::Name("Depth") } });
        AZ::RPI::Ptr<AZ::RPI::Pass> countPass;
        wireframePass && (countPass = passSystem->CreatePassFromRequest(&countRequest), true);
        countPass && (renderPipeline.AddPassAfter(countPass, AZ::Name("HammerWireframePass")), true);

        AZ::RPI::PassRequest resolveRequest;
        resolveRequest.m_passName = AZ::Name("HammerOverdrawResolvePass");
        resolveRequest.m_templateName = AZ::Name("HammerOverdrawResolvePassTemplate");
        resolveRequest.m_passEnabled = false;
        resolveRequest.AddInputConnection(AZ::RPI::PassConnection{
            AZ::Name("Count"), AZ::RPI::PassAttachmentRef{ AZ::Name("HammerOverdrawCountPass"), AZ::Name("Count") } });
        resolveRequest.AddInputConnection(AZ::RPI::PassConnection{
            AZ::Name("ColorInputOutput"),
            AZ::RPI::PassAttachmentRef{ AZ::Name("HammerWireframePass"), AZ::Name("ColorInputOutput") } });
        AZ::RPI::Ptr<AZ::RPI::Pass> resolvePass;
        countPass && (resolvePass = passSystem->CreatePassFromRequest(&resolveRequest), true);
        resolvePass && (renderPipeline.AddPassAfter(resolvePass, AZ::Name("HammerOverdrawCountPass")), true);

        AZ_Error(
            LogWindow, wireframePass && countPass && resolvePass,
            "Could not create the Hammer view-mode passes for pipeline '%s'", renderPipeline.GetId().GetCStr());

        (wireframePass && countPass && resolvePass) &&
            (m_pipelinePasses[&renderPipeline] = PipelinePasses{ wireframePass, countPass, resolvePass }, true);
    }

    void HammerViewModeFeatureProcessor::RefreshPipelinePasses(AZ::RPI::RenderPipeline& renderPipeline)
    {
        m_pipelinePasses.erase(&renderPipeline);

        auto* passSystem = AZ::RPI::PassSystemInterface::Get();
        auto wireframeFilter =
            AZ::RPI::PassFilter::CreateWithTemplateName(AZ::Name("HammerWireframePassTemplate"), &renderPipeline);
        auto countFilter =
            AZ::RPI::PassFilter::CreateWithTemplateName(AZ::Name("HammerOverdrawCountPassTemplate"), &renderPipeline);
        auto resolveFilter =
            AZ::RPI::PassFilter::CreateWithTemplateName(AZ::Name("HammerOverdrawResolvePassTemplate"), &renderPipeline);

        AZ::RPI::Ptr<AZ::RPI::Pass> wireframePass = passSystem->FindFirstPass(wireframeFilter);
        AZ::RPI::Ptr<AZ::RPI::Pass> countPass = passSystem->FindFirstPass(countFilter);
        AZ::RPI::Ptr<AZ::RPI::Pass> resolvePass = passSystem->FindFirstPass(resolveFilter);

        (wireframePass && countPass && resolvePass) &&
            (m_pipelinePasses[&renderPipeline] = PipelinePasses{ wireframePass, countPass, resolvePass }, true);
    }

    void HammerViewModeFeatureProcessor::OnRenderPipelineChanged(
        AZ::RPI::RenderPipeline* renderPipeline, AZ::RPI::SceneNotification::RenderPipelineChangeType changeType)
    {
        AZ_Assert(renderPipeline, "OnRenderPipelineChanged called with a null render pipeline");

        const bool removed = changeType == AZ::RPI::SceneNotification::RenderPipelineChangeType::Removed;
        removed && (m_pipelinePasses.erase(renderPipeline), true);
        !removed && (RefreshPipelinePasses(*renderPipeline), true);
    }

    bool HammerViewModeFeatureProcessor::AnyViewModePassEnabled() const
    {
        bool anyEnabled = false;
        for (const auto& [pipeline, passes] : m_pipelinePasses)
        {
            anyEnabled = anyEnabled || (passes.m_wireframe && passes.m_wireframe->IsEnabled()) ||
                (passes.m_count && passes.m_count->IsEnabled());
        }
        return anyEnabled;
    }

    void HammerViewModeFeatureProcessor::OnBeginPrepareRender()
    {
        const bool active = AnyViewModePassEnabled();
        (!active && !m_meshes.empty()) && (m_meshes.clear(), m_meshes.shrink_to_fit(), true);
        (active && EnsureAssets()) && (RefreshTrackedMeshes(), RefreshTransforms(), true);
    }

    void HammerViewModeFeatureProcessor::RefreshTrackedMeshes()
    {
        AZStd::vector<AZStd::pair<AZ::EntityId, AZ::Data::Instance<AZ::RPI::Model>>> current;
        AZ::ComponentApplicationBus::Broadcast(
            &AZ::ComponentApplicationRequests::EnumerateEntities,
            [&current](AZ::Entity* entity)
            {
                AZ::Data::Instance<AZ::RPI::Model> model;
                AZ::Render::MeshComponentRequestBus::EventResult(
                    model, entity->GetId(), &AZ::Render::MeshComponentRequests::GetModel);
                model && (current.emplace_back(entity->GetId(), model), true);
            });

        bool changed = current.size() != m_meshes.size();
        for (size_t i = 0; i < current.size() && !changed; ++i)
        {
            changed = current[i].first != m_meshes[i].m_entityId || current[i].second != m_meshes[i].m_model;
        }

        changed && (RebuildTrackedMeshes(current), true);

        for (TrackedMesh& mesh : m_meshes)
        {
            for (AZ::RPI::MeshDrawPacket& drawPacket : mesh.m_drawPackets)
            {
                drawPacket.Update(*GetParentScene(), false);
            }
        }
    }

    void HammerViewModeFeatureProcessor::RebuildTrackedMeshes(
        const AZStd::vector<AZStd::pair<AZ::EntityId, AZ::Data::Instance<AZ::RPI::Model>>>& current)
    {
        AZ_Assert(m_material, "RebuildTrackedMeshes called before the view-mode material was loaded");
        AZ_Assert(m_srgShaderAsset.IsReady(), "RebuildTrackedMeshes called before the object SRG shader was loaded");

        m_meshes.clear();
        m_meshes.reserve(current.size());

        for (const auto& [entityId, model] : current)
        {
            TrackedMesh mesh;
            mesh.m_entityId = entityId;
            mesh.m_model = model;
            mesh.m_objectSrg = AZ::RPI::ShaderResourceGroup::Create(m_srgShaderAsset, AZ::Name("HammerObjectSrg"));
            AZ_Error(LogWindow, mesh.m_objectSrg, "Failed to create a HammerObjectSrg for entity %s", entityId.ToString().c_str());

            const auto lods = model->GetLods();
            const size_t meshCount = (mesh.m_objectSrg && !lods.empty()) ? lods[0]->GetMeshes().size() : 0;
            for (size_t meshIndex = 0; meshIndex < meshCount; ++meshIndex)
            {
                AZ::RPI::MeshDrawPacket drawPacket(*lods[0], meshIndex, -1, m_material, mesh.m_objectSrg);
                drawPacket.Update(*GetParentScene(), true);
                mesh.m_drawPackets.push_back(AZStd::move(drawPacket));
            }

            mesh.m_objectSrg && (m_meshes.push_back(AZStd::move(mesh)), true);
        }
    }

    void HammerViewModeFeatureProcessor::RefreshTransforms()
    {
        for (TrackedMesh& mesh : m_meshes)
        {
            AZ::Transform world = AZ::Transform::CreateIdentity();
            AZ::TransformBus::EventResult(world, mesh.m_entityId, &AZ::TransformBus::Events::GetWorldTM);

            const bool changed = !world.IsClose(mesh.m_transform);
            changed &&
                (mesh.m_transform = world,
                 mesh.m_objectSrg->SetConstant(mesh.m_worldIndex, AZ::Matrix4x4::CreateFromTransform(world)),
                 mesh.m_objectSrg->Compile(), true);
        }
    }

    void HammerViewModeFeatureProcessor::Render(const RenderPacket& packet)
    {
        const bool active = AnyViewModePassEnabled() && !m_meshes.empty();
        active &&
            ([this, &packet]
             {
                 for (const AZ::RPI::ViewPtr& view : packet.m_views)
                 {
                     for (const TrackedMesh& mesh : m_meshes)
                     {
                         for (const AZ::RPI::MeshDrawPacket& drawPacket : mesh.m_drawPackets)
                         {
                             const AZ::RHI::DrawPacket* rhiDrawPacket = drawPacket.GetRHIDrawPacket();
                             rhiDrawPacket && (view->AddDrawPacket(rhiDrawPacket), true);
                         }
                     }
                 }
             }(),
             true);
    }
}
