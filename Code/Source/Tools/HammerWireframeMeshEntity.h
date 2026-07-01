#pragma once

#include "HammerWireframeDrawPacket.h"

#include <Atom/RPI.Reflect/Model/ModelLodIndex.h>
#include <AtomCore/Instance/Instance.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshHandleStateBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityBus.h>

namespace Hammer
{
    // Tracks one entity's Atom mesh (if any) and draws it in wireframe every frame.
    // Adapted from AZ::Render::DrawableMeshEntity (EditorModeFeedback gem).
    //
    // FeatureProcessor::Render() (and therefore Draw()) can run on a worker thread, not just the
    // main thread. Editor-only buses (AZ::TransformBus, AzFramework::EntityIdContextQueryBus -
    // the latter is used internally by RPI::Scene::GetSceneForEntityId) are not safe to query
    // concurrently with main-thread entity teardown (e.g. deleting a component, or an undo that
    // reloads a prefab instance and destroys/recreates the entity). To avoid this:
    //  - the world transform is cached via TransformNotificationBus (main-thread only)
    //  - the owning Scene is injected once at construction (main-thread only) instead of being
    //    re-derived from the entity ID via a bus lookup every frame
    //  - LOD selection (which needs a View, itself another bus-guarded lookup) is skipped
    //    entirely in favor of always using LOD 0, which is fine for a debug wireframe overlay
    //
    // MeshHandleStateNotificationBus only fires when the mesh handle itself is acquired or
    // released (e.g. toggling the entity's visibility). It does NOT fire when the model backing
    // an already-acquired handle changes in place (e.g. live White Box edits, swapping the mesh
    // asset on the component) - that case is covered by MeshComponentNotificationBus::OnModelReady
    // instead, which is explicitly documented as safe to receive from job threads (it uses a
    // real mutex, unlike TransformBus/EntityIdContextQueryBus).
    class HammerWireframeMeshEntity
        : private AZ::Render::MeshHandleStateNotificationBus::Handler
        , private AZ::Render::MeshComponentNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , private AzToolsFramework::EditorEntityVisibilityNotificationBus::Handler
    {
    public:
        HammerWireframeMeshEntity(AZ::EntityId entityId, AZ::Data::Instance<AZ::RPI::Material> material, AZ::RPI::Scene* scene);
        ~HammerWireframeMeshEntity();

        bool CanDraw() const;
        void Draw();

        // Retries mesh/model setup for entities whose model wasn't finished loading yet when
        // OnMeshHandleSet first fired. Safe to call repeatedly; no-ops once drawable.
        void RetryIfNotYetDrawable();

    private:
        // MeshHandleStateNotificationBus overrides ...
        void OnMeshHandleSet(const AZ::Render::MeshFeatureProcessorInterface::MeshHandle* meshHandle) override;

        // MeshComponentNotificationBus overrides ...
        void OnModelReady(const AZ::Data::Asset<AZ::RPI::ModelAsset>& modelAsset, const AZ::Data::Instance<AZ::RPI::Model>& model) override;
        void OnModelPreDestroy() override;

        // TransformNotificationBus overrides ...
        void OnTransformChanged(const AZ::Transform& localTM, const AZ::Transform& worldTM) override;

        // EditorEntityVisibilityNotificationBus overrides ...
        // Reflects the entity's *computed* visibility (own flag combined with ancestor/prefab
        // state), so hiding a parent entity or prefab instance hides this mesh too, not just
        // hiding the entity directly.
        void OnEntityVisibilityChanged(bool visibility) override;

        void CreateOrUpdateDrawPackets(
            AZ::Render::MeshFeatureProcessorInterface* featureProcessor, const AZ::Data::Instance<AZ::RPI::Model>& model);
        void ClearDrawData();
        void BuildDrawPackets(const AZ::Data::Asset<AZ::RPI::ModelAsset>& modelAsset, const AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>& objectSrg);
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> CreateObjectSrg(AZ::Render::MeshFeatureProcessorInterface* featureProcessor) const;

        AZ::EntityId m_entityId;
        AZ::RPI::Scene* m_scene = nullptr;
        const AZ::Render::MeshFeatureProcessorInterface::MeshHandle* m_meshHandle = nullptr;
        AZ::Data::Instance<AZ::RPI::Material> m_material;
        AZStd::vector<HammerWireframeDrawPacket> m_drawPackets;
        AZ::Transform m_cachedWorldTM = AZ::Transform::CreateIdentity();
        bool m_isVisible = true;
    };
} // namespace Hammer
