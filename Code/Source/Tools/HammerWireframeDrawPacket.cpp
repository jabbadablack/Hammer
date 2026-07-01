#include "HammerWireframeDrawPacket.h"

#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Shader/ShaderSystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI/RHISystemInterface.h>

namespace Hammer
{
    using namespace AZ;

    HammerWireframeDrawPacket::HammerWireframeDrawPacket(
        RPI::ModelLod& modelLod,
        size_t modelLodMeshIndex,
        Data::Instance<RPI::Material> material,
        Data::Instance<RPI::ShaderResourceGroup> objectSrg)
        : m_modelLod(&modelLod)
        , m_modelLodMeshIndex(modelLodMeshIndex)
        , m_objectSrg(objectSrg)
        , m_material(material)
    {
        RHI::RHISystemInterface* rhiSystem = RHI::RHISystemInterface::Get();
        RHI::DrawListTagRegistry* drawListTagRegistry = rhiSystem->GetDrawListTagRegistry();
        m_drawListTag = drawListTagRegistry->AcquireTag(Name("hammerwireframe"));
    }

    bool HammerWireframeDrawPacket::Update(const RPI::Scene& parentScene, bool forceUpdate)
    {
        // Also retry whenever the previous build produced nothing: Hammer's wireframe pipeline
        // may not have been added to the scene yet (its DrawListTag has no pass output until
        // then), and the material's change-id alone won't change again to trigger a rebuild.
        if (forceUpdate || !m_drawPacket || (!m_material->NeedsCompile() && m_materialChangeId != m_material->GetCurrentChangeId()))
        {
            DoUpdate(parentScene);
            m_materialChangeId = m_material->GetCurrentChangeId();
            return true;
        }

        return false;
    }

    bool HammerWireframeDrawPacket::DoUpdate(const RPI::Scene& parentScene)
    {
        AZStd::span<RPI::ModelLod::Mesh> meshes = m_modelLod->GetMeshes();
        RPI::ModelLod::Mesh& mesh = meshes[m_modelLodMeshIndex];

        RHI::DrawPacketBuilder drawPacketBuilder{ RHI::MultiDevice::AllDevices };
        drawPacketBuilder.Begin(nullptr);

        drawPacketBuilder.SetGeometryView(&mesh);
        drawPacketBuilder.AddShaderResourceGroup(m_objectSrg->GetRHIShaderResourceGroup());
        drawPacketBuilder.AddShaderResourceGroup(m_material->GetRHIShaderResourceGroup());

        ShaderList shaderList;
        shaderList.reserve(m_activeShaders.size());

        m_perDrawSrgs.clear();

        auto appendShader = [&](const RPI::ShaderCollection::Item& shaderItem, const Name& materialPipelineName)
        {
            if (!parentScene.HasOutputForPipelineState(m_drawListTag))
            {
                AZ_Warning("HammerWireframeDrawPacket", false, "HasOutputForPipelineState(hammerwireframe) is false; no pass in the scene currently outputs this DrawListTag");
                return false;
            }

            Data::Instance<RPI::Shader> shader = RPI::Shader::FindOrCreate(shaderItem.GetShaderAsset());
            if (!shader)
            {
                AZ_Error("HammerWireframeDrawPacket", false, "Failed to find or create shader instance");
                return false;
            }

            RPI::ShaderOptionGroup shaderOptions = *shaderItem.GetShaderOptions();
            shaderOptions.SetUnspecifiedToDefaultValues();

            m_modelLod->CheckOptionalStreams(
                shaderOptions,
                shader->GetInputContract(),
                m_modelLodMeshIndex,
                {},
                m_material->GetAsset()->GetMaterialTypeAsset()->GetUvNameMap());

            const RPI::ShaderVariantId finalVariantId = shaderOptions.GetShaderVariantId();
            const RPI::ShaderVariant& variant = shader->GetVariant(finalVariantId);

            RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
            variant.ConfigurePipelineState(pipelineStateDescriptor, shaderOptions);

            const RHI::RenderStates& renderStatesOverlay = *shaderItem.GetRenderStatesOverlay();
            RHI::MergeStateInto(renderStatesOverlay, pipelineStateDescriptor.m_renderStates);

            RPI::UvStreamTangentBitmask uvStreamTangentBitmask;
            RHI::StreamBufferIndices streamIndices;

            if (!m_modelLod->GetStreamsForMesh(
                pipelineStateDescriptor.m_inputStreamLayout,
                streamIndices,
                &uvStreamTangentBitmask,
                shader->GetInputContract(),
                m_modelLodMeshIndex,
                {},
                m_material->GetAsset()->GetMaterialTypeAsset()->GetUvNameMap()))
            {
                AZ_Warning("HammerWireframeDrawPacket", false, "GetStreamsForMesh failed; mesh geometry doesn't satisfy the shader's input contract");
                return false;
            }

            parentScene.ConfigurePipelineState(m_drawListTag, pipelineStateDescriptor);

            const RHI::PipelineState* pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
            if (!pipelineState)
            {
                AZ_Error("HammerWireframeDrawPacket", false, "Failed to acquire pipeline state");
                return false;
            }

            RHI::DrawPacketBuilder::DrawRequest drawRequest;
            drawRequest.m_listTag = m_drawListTag;
            drawRequest.m_pipelineState = pipelineState;
            drawRequest.m_streamIndices = streamIndices;

            if (materialPipelineName != RPI::MaterialPipelineNone)
            {
                RHI::DrawFilterTag pipelineTag = parentScene.GetDrawFilterTagRegistry()->AcquireTag(materialPipelineName);
                drawRequest.m_drawFilterMask = 1 << pipelineTag.GetIndex();
            }

            drawPacketBuilder.AddDrawItem(drawRequest);

            shaderList.emplace_back(AZStd::move(shader));

            return true;
        };

        int totalShaderItems = 0;
        int enabledShaderItems = 0;
        m_material->ForAllShaderItems(
            [&](const Name& materialPipelineName, const RPI::ShaderCollection::Item& shaderItem)
            {
                ++totalShaderItems;
                if (shaderItem.IsEnabled())
                {
                    ++enabledShaderItems;
                    appendShader(shaderItem, materialPipelineName);
                }
                return true;
            });

        m_drawPacket = drawPacketBuilder.End();

        AZ_Warning(
            "HammerWireframeDrawPacket", m_drawPacket,
            "DoUpdate failed to build a draw packet: totalShaderItems=%d enabledShaderItems=%d addedDrawItems=%zu",
            totalShaderItems, enabledShaderItems, shaderList.size());

        if (m_drawPacket)
        {
            m_activeShaders = shaderList;
            m_materialSrg = m_material->GetRHIShaderResourceGroup();
            return true;
        }

        return false;
    }

    const RHI::DrawPacket* HammerWireframeDrawPacket::GetRHIDrawPacket() const
    {
        return m_drawPacket.get();
    }
} // namespace Hammer
