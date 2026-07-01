#pragma once

#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Public/Model/ModelLod.h>
#include <Atom/RHI/DrawPacket.h>
#include <Atom/RHI/DrawPacketBuilder.h>

namespace AZ::RPI
{
    class Scene;
}

namespace Hammer
{
    // Builds an RHI DrawPacket for a single mesh, reusing its existing GPU geometry buffers
    // but rendering it with Hammer's own wireframe material instead of the mesh's assigned one.
    // Adapted from AZ::Render::EditorStateMeshDrawPacket (EditorModeFeedback gem).
    class HammerWireframeDrawPacket
    {
    public:
        using ShaderList = AZStd::vector<AZ::Data::Instance<AZ::RPI::Shader>>;

        HammerWireframeDrawPacket() = default;
        HammerWireframeDrawPacket(
            AZ::RPI::ModelLod& modelLod,
            size_t modelLodMeshIndex,
            AZ::Data::Instance<AZ::RPI::Material> material,
            AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> objectSrg);

        AZ_DEFAULT_COPY(HammerWireframeDrawPacket);
        AZ_DEFAULT_MOVE(HammerWireframeDrawPacket);

        bool Update(const AZ::RPI::Scene& parentScene, bool forceUpdate = false);

        const AZ::RHI::DrawPacket* GetRHIDrawPacket() const;

    private:
        bool DoUpdate(const AZ::RPI::Scene& parentScene);

        AZ::RPI::Ptr<AZ::RHI::DrawPacket> m_drawPacket;

        ShaderList m_activeShaders;

        AZ::Data::Instance<AZ::RPI::ModelLod> m_modelLod;
        size_t m_modelLodMeshIndex = 0;

        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> m_objectSrg;
        AZ::RPI::ConstPtr<AZ::RHI::ShaderResourceGroup> m_materialSrg;

        AZStd::fixed_vector<AZ::Data::Instance<AZ::RPI::ShaderResourceGroup>, AZ::RHI::DrawPacketBuilder::DrawItemCountMax> m_perDrawSrgs;

        AZ::Data::Instance<AZ::RPI::Material> m_material;
        AZ::RPI::Material::ChangeId m_materialChangeId = AZ::RPI::Material::DEFAULT_CHANGE_ID;

        AZ::RHI::DrawListTag m_drawListTag;
    };
} // namespace Hammer
