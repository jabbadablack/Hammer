#pragma once

#include <Atom/RHI.Reflect/ShaderInputNameIndex.h>
#include <Atom/RPI.Public/FeatureProcessor.h>
#include <Atom/RPI.Public/MeshDrawPacket.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

namespace Hammer
{
    class HammerViewModeFeatureProcessor final
        : public AZ::RPI::FeatureProcessor
    {
    public:
        AZ_CLASS_ALLOCATOR(HammerViewModeFeatureProcessor, AZ::SystemAllocator)
        AZ_RTTI(Hammer::HammerViewModeFeatureProcessor, "{8C1F4B7A-52D3-4E68-9B0E-A2F30D174C55}", AZ::RPI::FeatureProcessor);

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;
        void AddRenderPasses(AZ::RPI::RenderPipeline* renderPipeline) override;
        void Render(const RenderPacket& packet) override;

        void OnBeginPrepareRender() override;
        void OnRenderPipelineChanged(
            AZ::RPI::RenderPipeline* renderPipeline, AZ::RPI::SceneNotification::RenderPipelineChangeType changeType) override;

    private:
        struct TrackedMesh
        {
            AZ::EntityId m_entityId;
            AZ::Data::Instance<AZ::RPI::Model> m_model;
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_objectSrg;
            AZ::RHI::ShaderInputNameIndex m_worldIndex = "m_world";
            AZ::RHI::ShaderInputNameIndex m_colorIndex = "m_color";
            AZ::Transform m_transform = AZ::Transform::CreateUniformScale(0.0f);
            AZStd::vector<AZ::RPI::MeshDrawPacket> m_drawPackets;
        };

        struct PipelinePasses
        {
            AZ::RPI::Ptr<AZ::RPI::Pass> m_background;
            AZ::RPI::Ptr<AZ::RPI::Pass> m_wireframeHidden;
            AZ::RPI::Ptr<AZ::RPI::Pass> m_wireframe;
            AZ::RPI::Ptr<AZ::RPI::Pass> m_count;
            AZ::RPI::Ptr<AZ::RPI::Pass> m_resolve;
        };

        bool AnyViewModePassEnabled() const;
        void QueueAssetLoads();
        bool EnsureAssets();
        void InjectPasses(AZ::RPI::RenderPipeline& renderPipeline);
        void RefreshPipelinePasses(AZ::RPI::RenderPipeline& renderPipeline);
        void RefreshTrackedMeshes();
        void RebuildTrackedMeshes(const AZStd::vector<AZStd::pair<AZ::EntityId, AZ::Data::Instance<AZ::RPI::Model>>>& current);
        void RefreshTransforms();

        AZ::Data::Instance<AZ::RPI::Material> m_material;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
        AZ::Data::Asset<AZ::RPI::ShaderAsset> m_srgShaderAsset;
        AZStd::vector<TrackedMesh> m_meshes;
        AZStd::unordered_map<AZ::RPI::RenderPipeline*, PipelinePasses> m_pipelinePasses;
    };
}
