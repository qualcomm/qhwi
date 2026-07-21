//
//
//  test_depth_stencil_buffer.cpp
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
    float3 pos;
    float3 color;
};

class DepthStencilTestTask final : Task
{
public:
    DepthStencilTestTask(bool enableStencilTest)
        : m_enableStencilTest(enableStencilTest)
    {
    }

    struct InitParams
    {
        Device* device;
        uint32_t numVerts = 0;
        VertexPosColor* pVertexPosColorData = nullptr;
        uint32_t numIndices = 0;
        uint16_t* pIndicesData = nullptr;
        PrimitiveTopology primitiveTopology = PrimitiveTopology::eTriangleList;
        Format colorAttachmentFormat;
        Format depthStencilAttachmentFormat = Format::eUnknown;
        StencilOpState stencilOpState = {};
    };

    void init(const InitParams& p)
    {
        Task::init(p.device);

        m_primitiveTopology = p.primitiveTopology;
        m_numVerts = p.numVerts;
        m_numIndices = p.numIndices;

        m_bPosCol.initAsVertexBuffer(m_device, "posAndColor", p.pVertexPosColorData, p.numVerts * sizeof(VertexPosColor));
        if (m_numIndices)
        {
            m_bIndices.initAsIndexBuffer(m_device, "indices", p.pIndicesData, p.numIndices * sizeof(uint16_t));
        }
        initPipeline(p.colorAttachmentFormat, p.depthStencilAttachmentFormat, p.stencilOpState);
    }

    void uploadStagingBuffers(CommandList* cmdList)
    {
        m_bPosCol.uploadFromStagingBuffer(cmdList);
        m_bIndices.uploadFromStagingBuffer(cmdList);
    }

    void draw(CommandList* cmdList)
    {
        cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        cmdList->bindVertexBuffer(m_bPosCol.buffer.Get(), 0, 0, sizeof(VertexPosColor));
        cmdList->bindIndexBuffer(m_bIndices.buffer.Get(), IndexFormat::eUint16);
        cmdList->draw(m_numVerts);
    }

private:
    void initPipeline(Format colorAttachmentFormat, Format depthStencilAttachmentFormat, StencilOpState stencilOpState)
    {
        Shader1 vs, ps;

        initVertexShader(m_device, "testDepthStencilBuffer", vs, "VSMain");
        initPixelShader(m_device, "testDepthStencilBuffer", ps, "PSMain");

        m_graphicsPipelineLayout = m_device->createPipelineLayout({});

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
                                                                .depthStencilState = { .enableDepthTest = true,
                                                                                       .enableDepthWrite = true,
                                                                                       .depthCompareOp = CompareOp::eLess,
                                                                                       .enableStencilTest = m_enableStencilTest,
                                                                                       .stencilReadMask = 0xff,
                                                                                       .stencilWriteMask = 0xff,
                                                                                       .frontFace = stencilOpState,
                                                                                       .backFace = {},
                                                                                       .stencilReference = m_stencilRef },
                                                                .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                              .semanticIndex = 0,
                                                                                                                              .binding = 0,
                                                                                                                              .format = Format::eRgb32Float,
                                                                                                                              .offset = {},
                                                                                                                              .stride = sizeof(VertexPosColor),
                                                                                                                              .inputRate = {} },
                                                                                                                            { .semanticName = "COLOR",
                                                                                                                              .semanticIndex = 0,
                                                                                                                              .binding = 0,
                                                                                                                              .format = Format::eRgb32Float,
                                                                                                                              .offset = sizeof(float3),
                                                                                                                              .stride = sizeof(VertexPosColor),
                                                                                                                              .inputRate = {} } } },
                                                                .viewportState = {},
                                                                .scissorState = {},
                                                                .layout = m_graphicsPipelineLayout,
                                                                .primitiveTopology = m_primitiveTopology,
                                                                .primitiveRestartValue = {},
                                                                .colorAttachmentFormats = { colorAttachmentFormat },
                                                                .depthStencilFormat = depthStencilAttachmentFormat,
                                                                .meshShaderParams = {},
                                                                .multiViewState = {},
                                                                .colorAttachmentRemapping = {},
                                                                .name = {} });
    }

    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPosCol;
    Buffer1 m_bIndices;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
    PrimitiveTopology m_primitiveTopology;
    bool m_enableStencilTest = false;
    uint32_t m_stencilRef = 128;
};

class TestDepthStencilBuffer : public UnitTest
{
public:
    TestDepthStencilBuffer(GraphicsBackend graphicsBackend,
                           Config config,
                           const char* name,
                           bool enableStencilTest = false,
                           DepthStencilType depthStencilType = DepthStencilType::eDepthOnly)
        : UnitTest(graphicsBackend, config, name)
        , m_DepthStencilTestTask(enableStencilTest)
        , m_stencilTestTask(enableStencilTest)
    {
        m_config.depthStencilType = depthStencilType;
        m_enableStencilTest = enableStencilTest;
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();

        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        // Depth test task
        {
            VertexPosColor vertexPosColor[] = { { { -1.0f, -1.0f, 0.8f }, { -1.0f, 1.0f, 0.0f } }, { { -1.0f, 1.0f, 0.8f }, { -1.0f, 1.0f, 0.0f } },
                                                { { 1.0f, -1.0f, 0.5f }, { -1.0f, 1.0f, 0.0f } },  { { -1.0f, -1.0f, 0.5f }, { -1.0f, 0.0f, 1.0f } },
                                                { { 1.0f, 1.0f, 0.8f }, { -1.0f, 0.0f, 1.0f } },   { { 1.0f, -1.0f, 0.8f }, { -1.0f, -1.0f, 1.0f } } };

            uint16_t indices[] = { 0, 1, 2, 3, 4, 5 };

            m_DepthStencilTestTask.init({ .device = m_device.Get(),
                                          .numVerts = sizeof(vertexPosColor) / sizeof(vertexPosColor[0]),
                                          .pVertexPosColorData = vertexPosColor,
                                          .numIndices = sizeof(indices) / sizeof(indices[0]),
                                          .pIndicesData = indices,
                                          .primitiveTopology = PrimitiveTopology::eTriangleList,
                                          .colorAttachmentFormat = m_colorAttachmentFormat,
                                          .depthStencilAttachmentFormat = m_depthStencilAttachmentFormat,
                                          .stencilOpState = { .failOp = StencilOp::eZero, .depthFailOp = {}, .passOp = {}, .compareOp = CompareOp::eEqual } });

            beginTransfer();
            m_DepthStencilTestTask.uploadStagingBuffers(m_copyCommandList.Get());
            endTransfer();
        }

        if (m_config.depthStencilType == DepthStencilType::eDepthStencil)
        {
            VertexPosColor vertexPosColor[] = { { { 0.00f, -1.00f, 0.9f }, { 1.0f, 1.0f, 1.0f } },
                                                { { -0.25f, -0.25f, 0.9f }, { 1.0f, 1.0f, 1.0f } },
                                                { { 0.25f, -0.25f, 0.9f }, { 1.0f, 1.0f, 1.0f } } };

            uint16_t indices[] = { 0, 1, 2 };

            m_stencilTestTask.init({ .device = m_device.Get(),
                                     .numVerts = sizeof(vertexPosColor) / sizeof(vertexPosColor[0]),
                                     .pVertexPosColorData = vertexPosColor,
                                     .numIndices = sizeof(indices) / sizeof(indices[0]),
                                     .pIndicesData = indices,
                                     .primitiveTopology = PrimitiveTopology::eTriangleList,
                                     .colorAttachmentFormat = m_colorAttachmentFormat,
                                     .depthStencilAttachmentFormat = m_depthStencilAttachmentFormat,
                                     .stencilOpState = { .failOp = {}, .depthFailOp = {}, .passOp = StencilOp::eZero, .compareOp = CompareOp::eEqual } });

            beginTransfer();
            m_stencilTestTask.uploadStagingBuffers(m_copyCommandList.Get());
            endTransfer();
        }

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        if (m_config.depthStencilType == DepthStencilType::eDepthStencil)
        {
            m_stencilTestTask.draw(commandList);
        }
        m_DepthStencilTestTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    DepthStencilTestTask m_DepthStencilTestTask;
    DepthStencilTestTask m_stencilTestTask;
    bool m_enableStencilTest = false;
};

UnitTest* create_test_depth_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDepthStencilBuffer(backend, config, "test_depth_buffer");
}

UnitTest* create_test_stencil_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDepthStencilBuffer(backend, config, "test_stencil_buffer", true, UnitTest::DepthStencilType::eDepthStencil);
}
