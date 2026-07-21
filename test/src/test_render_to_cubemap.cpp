//
//
//  test_to_cubemap.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
#include "test_render_to_cubemap.h"
using namespace qhwi;
using namespace test_util;

constexpr uint32_t NumColorAttachments = 6;
constexpr uint32_t CubeWidthInPixels = 640;

void RenderToCubemapTask::init(Device* device)
{
    Task::init(device);

    m_device = device;
    m_cubemap.init(m_device,
                   { .heapFlags = {},
                     .w = CubeWidthInPixels,
                     .h = CubeWidthInPixels,
                     .d = 1,
                     .mipLevels = {},
                     .arrayLayers = NumColorAttachments,
                     .numPlanes = 1,
                     .numSamples = SampleCount::e1,
                     .format = Format::eRgba8Unorm,
                     .rSwizzle = {},
                     .gSwizzle = ComponentSwizzle::eG,
                     .bSwizzle = ComponentSwizzle::eB,
                     .aSwizzle = ComponentSwizzle::eA,
                     .usage = TextureUsage_Sampled | TextureUsage_ColorAttachment,
                     .type = TextureType::eCube,
                     .name = "cubemap",
                     .automaticBarriers = true },
                   nullptr,
                   true);

    Quad quad;
    float2* posData;

    quad.init();
    posData = quad.pos;
    m_numVerts = quad.NumVerts;
    m_bPos.initAsVertexBuffer(m_device, "pos", posData, m_numVerts * sizeof(float2));

    initPipeline();
}

void RenderToCubemapTask::initPipeline()
{
    Shader1 vs, ps;

    initVertexShader(m_device, "testRenderToCubemap", vs, "VSMain");
    initPixelShader(m_device, "testRenderToCubemap", ps, "PSMain");

    m_graphicsPipelineLayout = m_device->createPipelineLayout({});
    m_graphicsPipeline = m_device->createGraphicsPipeline(
        { .vertexShader = vs.shader,
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
                                                                        .binding = 0,
                                                                        .format = Format::eRg32Float,
                                                                        .offset = {},
                                                                        .stride = sizeof(float2),
                                                                        .inputRate = {} } } },
          .viewportState = {},
          .scissorState = {},
          .layout = m_graphicsPipelineLayout,
          .primitiveTopology = PrimitiveTopology::eTriangleStrip,
          .primitiveRestartValue = {},
          .colorAttachmentFormats = { Format::eRgba8Unorm, Format::eRgba8Unorm, Format::eRgba8Unorm, Format::eRgba8Unorm, Format::eRgba8Unorm, Format::eRgba8Unorm },
          .depthStencilFormat = {},
          .meshShaderParams = {},
          .multiViewState = {},
          .colorAttachmentRemapping = {},
          .name = {} });
}

void RenderToCubemapTask::draw(CommandList* commandList)
{
    ColorAttachmentDescription colorAttachmentDescs[NumColorAttachments] = {};
    for (uint32_t i = 0; i < NumColorAttachments; ++i)
    {
        colorAttachmentDescs[i] = { .textureData = TextureAttachmentData{ .texture = m_cubemap.get(),
                                                                          .subresource = { .mipSlice = {}, .arraySlice = static_cast<uint16_t>(i), .planeSlice = {} } },
                                    .resolveTarget = {},
                                    .loadOp = AttachmentLoadOp::eClear,
                                    .storeOp = AttachmentStoreOp::eStore,
                                    .clearValue = {},
                                    .initialState = TextureState_ColorAttachment,
                                    .finalState = TextureState_ColorAttachment };
    }

    commandList->beginRenderPass(NumColorAttachments, colorAttachmentDescs);
    commandList->setViewport({ .w = float(CubeWidthInPixels), .h = float(CubeWidthInPixels) });
    commandList->setScissor({ .w = CubeWidthInPixels, .h = CubeWidthInPixels });
    commandList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    commandList->bindVertexBuffer(m_bPos.get(), 0, 0, sizeof(float2));
    commandList->draw(m_numVerts);
    commandList->endRenderPass();
}

TestRenderToCubemap::TestRenderToCubemap(GraphicsBackend graphicsBackend, Config config, const char* testName, Format format, const void* srcData)
    : UnitTest(graphicsBackend, config, testName)
{
    m_pSrcData = srcData;
    m_format = format;
}

ResultCode TestRenderToCubemap::init()
{
    UnitTest::init();

    float2 vertices[] = {
        // -X
        { -1.0f, -0.33f },
        { -1.0f, 0.34f },
        { -0.5f, +0.34f },
        { -1.0f, -0.33f },
        { -0.5f, 0.34f },
        { -0.5f, -0.33f },

        // +Z
        { -0.5f, -0.33f },
        { -0.5f, 0.34f },
        { 0.0f, +0.34f },
        { -0.5f, -0.33f },
        { +0.0f, 0.34f },
        { 0.0f, -0.33f },

        // +X
        { 0.0f, -0.33f },
        { 0.0f, 0.34f },
        { 0.5f, +0.34f },
        { 0.0f, -0.33f },
        { 0.5f, 0.34f },
        { 0.5f, -0.33f },

        // -Z
        { 0.5f, -0.33f },
        { 0.5f, 0.34f },
        { 1.0f, +0.34f },
        { 0.5f, -0.33f },
        { 1.0f, 0.34f },
        { 1.0f, -0.33f },

        // +Y
        { -0.5f, 0.34f },
        { -0.5f, 1.0f },
        { 0.0f, 1.00f },
        { -0.5f, 0.34f },
        { +0.0f, 1.0f },
        { 0.0f, 0.34f },

        // -Y
        { -0.5f, -1.0f },
        { -0.5f, -0.33f },
        { 0.0f, -0.33f },
        { -0.5f, -1.0f },
        { +0.0f, -0.33f },
        { 0.0f, -1.00f },
    };

    float3 uvwData[] = {
        // -X
        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },

        // +Z
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },

        // +X
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },

        // -Z
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },

        // +Y
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },

        // -Y
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
    };

    m_renderToCubemapTask.init(m_device.Get());
    m_drawTextureTask.init({ .device = m_device.Get(),
                             .texture = m_renderToCubemapTask.getCubeMap(),
                             .posData = vertices,
                             .numVerts = sizeof(vertices) / sizeof(vertices[0]),
                             .indicesData = {},
                             .numIndices = {},
                             .uvwData = uvwData,
                             .samplerDesc = {},
                             .colorAttachmentFormat = m_colorAttachmentFormat,
                             .depthStencilFormat = m_depthStencilAttachmentFormat });

    beginTransfer();
    m_renderToCubemapTask.uploadStagingBuffers(m_copyCommandList.Get());
    m_drawTextureTask.uploadStagingBuffers(m_copyCommandList.Get());
    endTransfer();

    return ResultCode::eSuccess;
}

void TestRenderToCubemap::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    if (!m_renderedToCubemap)
    {
        m_renderToCubemapTask.draw(commandList);
        m_renderedToCubemap = true;
    }
    beginRenderPass(commandList);
    m_drawTextureTask.draw(commandList);
    endRenderPass(commandList);
    commandList->end();
}

UnitTest* create_test_render_to_cubemap(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestRenderToCubemap(backend, config, "test_to_cubemap", Format::eRgba8Unorm, nullptr);
}
