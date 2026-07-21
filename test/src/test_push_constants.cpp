//
//
//  test_push_constants.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestPushConstantsTask : public Task
{
public:
    struct InitParams
    {
        Device* device;
        float4 color;
        float2* posData;
        uint32_t numVerts;
        PrimitiveTopology primitiveTopology;
        Format colorAttachmentFormat;
        Format depthStencilFormat = Format::eUnknown;
    };

    void init(const InitParams& p)
    {
        Task::init(p.device);
        m_color = p.color;
        m_primitiveTopology = p.primitiveTopology;
        m_numVerts = p.numVerts;

        Quad quad;
        float2* posData = p.posData;
        uint32_t numVerts = p.numVerts;
        if (!posData)
        {
            quad.init();
            posData = quad.pos;
            numVerts = quad.NumVerts;
        }
        m_bPos.initAsVertexBuffer(m_device, "pos", posData, numVerts * sizeof(float2));
        initPipeline(p.colorAttachmentFormat, p.depthStencilFormat);
    }

    void uploadStagingBuffers(CommandList* cmdList) { m_bPos.uploadFromStagingBuffer(cmdList); }

    void draw(CommandList* cmdList)
    {
        cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
        cmdList->pushGraphicsConstants(&m_color);
        cmdList->draw(m_numVerts);
    }

protected:
    void initPipeline(Format colorAttachmentFormat, Format depthStencilFormat)
    {
        m_graphicsPipelineLayout = m_device->create(
            PipelineLayoutDesc().addPushConstantRange({ .shaderRegister = {}, .registerSpace = {}, .shaderStages = ShaderStage_Vertex, .offset = {}, .size = sizeof(CbData) }));

        Shader1 vs, ps;
        initVertexShader(m_device, "testPushConstants", vs);
        initPixelShader(m_device, "testPushConstants", ps);

        m_graphicsPipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                                .pixelShader = ps.shader,
                                                                .domainShader = {},
                                                                .hullShader = {},
                                                                .geometryShader = {},
                                                                .amplificationShader = {},
                                                                .meshShader = {},
                                                                .blendState = {},
                                                                .rasterizerState = {},
                                                                .multisampleState = {},
                                                                .depthStencilState = {},
                                                                .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                              .semanticIndex = 0,
                                                                                                                              .binding = B_POS,
                                                                                                                              .format = Format::eRg32Float,
                                                                                                                              .offset = {},
                                                                                                                              .stride = sizeof(float2),
                                                                                                                              .inputRate = {} } } },
                                                                .viewportState = {},
                                                                .scissorState = {},
                                                                .layout = m_graphicsPipelineLayout,
                                                                .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                                                .primitiveRestartValue = {},
                                                                .colorAttachmentFormats = { colorAttachmentFormat },
                                                                .depthStencilFormat = depthStencilFormat,
                                                                .meshShaderParams = {},
                                                                .multiViewState = {},
                                                                .colorAttachmentRemapping = {},
                                                                .name = {} });
    }

    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPos;
    static const uint32_t B_POS = 0;
    uint32_t m_numVerts = 0;
    PrimitiveTopology m_primitiveTopology;
    float4 m_color;

    struct CbData
    {
        float4 color;
    };
};

class TestPushConstants : public UnitTest
{
public:
    TestPushConstants(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_push_constants")
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
        const int NumVerts = 3;
        const float4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
        float2 pos[NumVerts] = { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };
        m_pushConstantsTask.init({ .device = m_device.Get(),
                                   .color = color,
                                   .posData = pos,
                                   .numVerts = NumVerts,
                                   .primitiveTopology = PrimitiveTopology::eLineStrip,
                                   .colorAttachmentFormat = m_colorAttachmentFormat,
                                   .depthStencilFormat = m_depthStencilAttachmentFormat });

        beginTransfer();
        m_pushConstantsTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_pushConstantsTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    TestPushConstantsTask m_pushConstantsTask;
};

UnitTest* create_test_push_constants(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestPushConstants(backend, config);
}
