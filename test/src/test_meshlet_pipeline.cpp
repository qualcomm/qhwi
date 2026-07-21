//
//
//  test_meshlet_pipeline.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

struct VertexPosColor
{
    float4 pos;
    float4 col;
};

static inline uint32_t compMul(const uint32_t* v)
{
    return v[0] * v[1] * v[2];
}

class TestMeshletPipeline : public UnitTest
{
public:
    TestMeshletPipeline(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_meshlet_pipeline")
    {
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        if (!m_device->supportsMeshShaders())
        {
            return ResultCode::eUnsupported;
        }

        VertexPosColor verticesPosCol[] = {
            { { -1.0, 0.5, 0.5, 1.0 }, { 1.0, 0.0, 0.0, 1.0 } },
            { { -0.75, 1.0, 0.5, 1.0 }, { 0.0, 1.0, 0.0, 1.0 } },
            { { -0.5, 0.5, 0.5, 1.0 }, { 0.0, 0.0, 1.0, 1.0 } },
        };

        uint32_t indices[] = { 0 | (1 << 8) | (2 << 16) };

        m_bVerticesPosCol.initAsUnorderedAccessBuffer(m_device.Get(), "vertices", verticesPosCol, sizeof(verticesPosCol));
        m_bIndices.initAsUnorderedAccessBuffer(m_device.Get(), "indices", indices, sizeof(indices));

        initPipelines();

        beginTransfer();
        m_bVerticesPosCol.uploadFromStagingBuffer(m_copyCommandList.Get());
        m_bIndices.uploadFromStagingBuffer(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void initPipelines()
    {
        m_device->createDescriptorSetAndLayout(
            DescriptorSetDesc().addStructuredBuffer(0, m_bVerticesPosCol.buffer, sizeof(VertexPosColor)).addStructuredBuffer(1, m_bIndices.buffer, sizeof(uint32_t)),
            ShaderStage_Mesh,
            0,
            m_descriptorSetLayouts[0],
            m_descriptorSets[0]);

        Shader1 as;
        Shader1 ms;
        Shader1 ps;

        initAmplificationShader(m_device.Get(), "testMeshletPipeline", as);
        initMeshShader(m_device.Get(), "testMeshletPipeline", ms);
        initPixelShader(m_device.Get(), "testMeshletPipeline", ps);

        m_pipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_graphicsPipeline =
            m_device->createGraphicsPipeline({ .vertexShader = {},
                                               .pixelShader = ps.shader,
                                               .domainShader = {},
                                               .hullShader = {},
                                               .geometryShader = {},
                                               .amplificationShader = as.shader,
                                               .meshShader = ms.shader,
                                               .blendState = {},
                                               .rasterizerState = {},
                                               .multisampleState = {},
                                               .depthStencilState = {},
                                               .vertexInputState = {},
                                               .viewportState = {},
                                               .scissorState = {},
                                               .layout = m_pipelineLayout,
                                               .primitiveTopology = PrimitiveTopology::eTriangleList,
                                               .primitiveRestartValue = {},
                                               .colorAttachmentFormats = { m_colorAttachmentFormat },
                                               .depthStencilFormat = {},
                                               .meshShaderParams = { .maxThreadsPerAmplificationThreadgroup = compMul(NumThreadsPerAmplificationShaderThreadGroup.data()),
                                                                     .maxThreadsPerMeshThreadgroup = compMul(NumThreadsPerMeshThreadGroup.data()) },
                                               .multiViewState = {},
                                               .colorAttachmentRemapping = {},
                                               .name = {} });
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        commandList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        commandList->bindGraphicsDescriptorSet(m_descriptorSets[0].Get());
        commandList->dispatchMesh({ NumAmplificationShaderThreadGroups, NumThreadsPerAmplificationShaderThreadGroup, NumThreadsPerMeshThreadGroup });
        endRenderPass(commandList);
        commandList->end();
    }

    qre::RefCountPtr<PipelineLayout> m_pipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    static constexpr std::array<uint32_t, 3> NumAmplificationShaderThreadGroups = { 4, 1, 1 };
    static constexpr std::array<uint32_t, 3> NumThreadsPerAmplificationShaderThreadGroup = { 1, 1, 1 };
    static constexpr std::array<uint32_t, 3> NumThreadsPerMeshThreadGroup = { 4, 1, 1 };
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<DescriptorSet> m_descriptorSets[NumDescriptorSets];
    Buffer1 m_bVerticesPosCol;
    Buffer1 m_bIndices;
};

UnitTest* create_test_meshlet_pipeline(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestMeshletPipeline(backend, config);
}
