#pragma once

#include "HammerWireframeMeshEntity.h"

#include <Atom/RPI.Public/FeatureProcessor.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Hammer
{
    // Draws every mesh entity in the editor's level as wireframe, for Hammer's preview viewport.
    // Adapted from AZ::Render::EditorModeFeatureProcessor (EditorModeFeedback gem).
    class HammerWireframeFeatureProcessor
        : public AZ::RPI::FeatureProcessor
        , private AzToolsFramework::EditorEntityContextNotificationBus::Handler
        , private AZ::TickBus::Handler
    {
    public:
        AZ_RTTI(Hammer::HammerWireframeFeatureProcessor, "{2B6F0F8C-9B0E-4A0F-8B8B-9A1B4E7B6C3D}", AZ::RPI::FeatureProcessor);
        AZ_CLASS_ALLOCATOR(HammerWireframeFeatureProcessor, AZ::SystemAllocator);
        AZ_DISABLE_COPY_MOVE(HammerWireframeFeatureProcessor);

        HammerWireframeFeatureProcessor() = default;
        ~HammerWireframeFeatureProcessor() override = default;

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;
        void Render(const RenderPacket& packet) override;

    private:
        // EditorEntityContextNotificationBus overrides ...
        void OnEditorEntityCreated(const AZ::EntityId& entityId) override;
        void OnEditorEntityDeleted(const AZ::EntityId& entityId) override;

        // TickBus overrides ...
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void TrackEntity(AZ::EntityId entityId);

        // Periodically re-enumerates all editor-context entities and tracks any not already
        // known. Level/prefab entities are bulk-instantiated and don't reliably fire
        // OnEditorEntityCreated, so a one-time scan at Activate() (which also runs before any
        // level is even loaded) misses them entirely; this makes discovery robust regardless of
        // how or when entities appear.
        void ScanForNewEntities();

        // Removes tracked entries whose entity no longer exists (see .cpp for why this is needed
        // in addition to OnEditorEntityDeleted).
        void PruneDeadEntities();

        AZ::Data::Instance<AZ::RPI::Material> m_material;
        AZStd::unordered_map<AZ::EntityId, AZStd::unique_ptr<HammerWireframeMeshEntity>> m_meshEntities;
        float m_entityScanTimer = 0.0f;
    };
} // namespace Hammer
