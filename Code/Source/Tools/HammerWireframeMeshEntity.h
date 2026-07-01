#pragma once

#include "HammerWireframeDrawPacket.h"

#include <Atom/RPI.Reflect/Model/ModelLodIndex.h>
#include <AtomCore/Instance/Instance.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshHandleStateBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/containers/vector.h>

namespace Hammer
{
    // Tracks one entity's Atom mesh (if any) and draws it in wireframe every frame.
    // Adapted from AZ::Render::DrawableMeshEntity (EditorModeFeedback gem).
    //
    // FeatureProcessor::Render() (and therefore Draw()) can run on a worker thread, not just the
    // main thread. AZ::TransformBus is not safe to query concurrently with main-thread entity
    // teardown (e.g. deleting a component), so the world transform is cached here via
    // TransformNotificationBus (which only ever fires on the main thread) instead of being
    // queried live from Draw().
    class HammerWireframeMeshEntity
        : private AZ::Render::MeshHandleStateNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        HammerWireframeMeshEntity(AZ::EntityId entityId, AZ::Data::Instance<AZ::RPI::Material> material);
        ~HammerWireframeMeshEntity();

        bool CanDraw() const;
        void Draw();

        // Retries mesh/model setup for entities whose model wasn't finished loading yet when
        // OnMeshHandleSet first fired. Safe to call repeatedly; no-ops once drawable.
        void RetryIfNotYetDrawable();

    private:
        AZ::RPI::ModelLodIndex GetModelLodIndex(const AZ::RPI::ViewPtr view, AZ::Data::Instance<AZ::RPI::Model> model) const;

        // MeshHandleStateNotificationBus overrides ...
        void OnMeshHandleSet(const AZ::Render::MeshFeatureProcessorInterface::MeshHandle* meshHandle) override;

        // TransformNotificationBus overrides ...
        void OnTransformChanged(const AZ::Transform& localTM, const AZ::Transform& worldTM) override;

        void CreateOrUpdateDrawPackets(
            const AZ::Render::MeshFeatureProcessorInterface* featureProcessor,
            AZ::RPI::ModelLodIndex modelLodIndex,
            AZ::Data::Instance<AZ::RPI::Model> model);
        void ClearDrawData();
        void BuildDrawPackets(AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset, AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> objectSrg);
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> CreateObjectSrg(const AZ::Render::MeshFeatureProcessorInterface* featureProcessor) const;

        AZ::EntityId m_entityId;
        const AZ::Render::MeshFeatureProcessorInterface::MeshHandle* m_meshHandle = nullptr;
        AZ::Data::Instance<AZ::RPI::Material> m_material;
        AZ::RPI::ModelLodIndex m_modelLodIndex = AZ::RPI::ModelLodIndex::Null;
        AZStd::vector<HammerWireframeDrawPacket> m_drawPackets;
        AZ::Transform m_cachedWorldTM = AZ::Transform::CreateIdentity();
    };
} // namespace Hammer
