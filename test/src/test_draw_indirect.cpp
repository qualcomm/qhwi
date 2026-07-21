//
//
//  test_draw_indirect.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#include "test_draw_indirect.h"

constexpr uint32_t DrawIndirectStride = 16;
constexpr uint32_t DrawIndexedIndirectStride = 20;

void DrawIndirectTask::init(UnitTest* parent, const InitParams& p)
{
    m_parent = parent;
    Task::init(p.device);
    m_color = p.color;
    m_primitiveTopology = p.primitiveTopology;
    m_numVerts = p.numVerts;
    m_numIndices = p.numIndices;
    m_numInstances = p.numInstances;

    m_vertexBuffer.initAsVertexBuffer(m_device, "vertex", p.vertexPosAndColor, p.numVerts * sizeof(VertexPosColor));
    m_instanceBuffer.initAsVertexBuffer(m_device, "instance", p.instancePosAndColor, p.numInstances * sizeof(InstancePosColor));
    if (m_numIndices)
    {
        m_bIndices.initAsIndexBuffer(m_device, "indices", p.indicesData, p.numIndices * sizeof(uint16_t));
    }
    initGraphicsPipeline(p.colorAttachmentFormat, p.depthStencilFormat);
    initComputePipeline(p.indirectArgBuffer, p.constantBuffer);
}

void DrawIndirectTask::uploadStagingBuffers(CommandList* cmdList)
{
    m_vertexBuffer.uploadFromStagingBuffer(cmdList);

    m_instanceBuffer.uploadFromStagingBuffer(cmdList);

    if (m_numIndices)
    {
        m_bIndices.uploadFromStagingBuffer(cmdList);
    }
}

void DrawIndirectTask::draw(CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_vertexBuffer.get(), B_VertexBuffer, 0, sizeof(VertexPosColor));
    cmdList->bindVertexBuffer(m_instanceBuffer.get(), B_InstanceBuffer, 0, sizeof(InstancePosColor));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), IndexFormat::eUint16);
        cmdList->drawIndexed(m_numIndices, m_numInstances);
    }
    else
    {
        cmdList->draw(m_numVerts, m_numInstances);
    }
}

void DrawIndirectTask::dispatch(CommandList* cmdList, uint32_t groupCountX)
{
    cmdList->beginComputePass();
    cmdList->bindComputePipeline(m_computePipeline.Get());
    cmdList->bindDescriptorSet(m_descriptorSet.Get(), qhwi::PipelineBindPoint::eCompute, m_computePipelineLayout.Get());

    cmdList->dispatch(groupCountX, 1, 1, NumThreadsPerGroup[0], NumThreadsPerGroup[1], NumThreadsPerGroup[2]);
    cmdList->endComputePass();
}

void DrawIndirectTask::drawIndirect(CommandList* cmdList, Buffer* buffer, uint32_t drawCount)
{
    m_parent->beginRenderPass(cmdList);
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_vertexBuffer.get(), B_VertexBuffer, 0, sizeof(VertexPosColor));
    cmdList->bindVertexBuffer(m_instanceBuffer.get(), B_InstanceBuffer, 0, sizeof(InstancePosColor));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), IndexFormat::eUint16);
        cmdList->drawIndexedIndirect(buffer, 0, drawCount);
    }
    else
    {
        cmdList->drawIndirect(buffer, 0, drawCount);
    }
    m_parent->endRenderPass(cmdList);
}

void DrawIndirectTask::initComputePipeline(Buffer* indirectArgBuffer, Buffer* constantBuffer)
{
    Shader1 cs;
    (m_numIndices == 0) ? initComputeShader(m_device, "testDrawIndirect", cs, "CSMain") : initComputeShader(m_device, "testDrawIndexedIndirect", cs, "CSMain");

    m_device->createDescriptorSetAndLayout(DescriptorSetDesc()
                                               .addConstantBuffer(0, constantBuffer)
                                               .addRWStructuredBuffer(0, indirectArgBuffer, (m_numIndices == 0) ? DrawIndirectStride : DrawIndexedIndirectStride)
                                               .setName((m_numIndices == 0) ? "indirectDescriptorSet" : "indexedIndirectDescriptorSet"),
                                           ShaderStage_Compute,
                                           0,
                                           m_descriptorSetLayout,
                                           m_descriptorSet);

    m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayout));

    m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
}

void DrawIndirectTask::initGraphicsPipeline(Format colorAttachmentFormat, Format depthStencilFormat)
{
    Shader1 vs, ps;
    initShader(m_device, "testDrawIndirect", "VSMain", ShaderType::eVertex, vs);
    initShader(m_device, "testDrawIndirect", "PSMain", ShaderType::ePixel, ps);

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
                                                            .depthStencilState = {},
                                                            .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_VertexBuffer,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(VertexPosColor),
                                                                                                                          .inputRate = {} },
                                                                                                                        { .semanticName = "COLOR",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_VertexBuffer,
                                                                                                                          .format = Format::eRgba32Float,
                                                                                                                          .offset = sizeof(float2),
                                                                                                                          .stride = sizeof(VertexPosColor),
                                                                                                                          .inputRate = {} },
                                                                                                                        { .semanticName = "INSTANCEPOSITION",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_InstanceBuffer,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(InstancePosColor),
                                                                                                                          .inputRate = VertexInputRate::eInstance },
                                                                                                                        { .semanticName = "INSTANCECOLOR",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = B_InstanceBuffer,
                                                                                                                          .format = Format::eRgba32Float,
                                                                                                                          .offset = sizeof(float2),
                                                                                                                          .stride = sizeof(InstancePosColor),
                                                                                                                          .inputRate = VertexInputRate::eInstance } } },
                                                            .viewportState = {},
                                                            .scissorState = {},
                                                            .layout = m_graphicsPipelineLayout,
                                                            .primitiveTopology = m_primitiveTopology,
                                                            .primitiveRestartValue = {},
                                                            .colorAttachmentFormats = { colorAttachmentFormat },
                                                            .depthStencilFormat = depthStencilFormat,
                                                            .meshShaderParams = {},
                                                            .multiViewState = {},
                                                            .colorAttachmentRemapping = {},
                                                            .name = {} });
}

TestDrawIndexedIndirect::TestDrawIndexedIndirect(GraphicsBackend graphicsBackend, Config config)
    : UnitTest(graphicsBackend, config, "test_draw_indexed_indirect")
{
}

ResultCode TestDrawIndexedIndirect::init()
{
    ResultCode resultCode = UnitTest::init();
    if (resultCode != ResultCode::eSuccess)
    {
        return resultCode;
    }

    // Create vertex buffer
    const int numInstancesX = 24;
    const int numInstancesY = 4;
    const float offsetY = 2.0f / numInstancesY;

    const int numEdges = 4;
    const int numVertices = numEdges + 1;
    const int numIndices = numEdges * 3;

    const float angle = 2 * float(M_PI) / (numVertices - 1);
    const float radius = 2.0f / (numInstancesX * 2.0f);
    const float4 color = { 0.5f, 0.0f, 0.5f, 1.0f };

    VertexPosColor vertices[numVertices];
    InstancePosColor instances[numInstancesY * numInstancesX];
    uint16_t indices[numIndices];
    const CB constantBufferData = { .numIndices = numIndices, .numInstances = numInstancesX };

    // Center of the hexagon
    vertices[numVertices - 1] = { { 0.0, 0.0 }, color };

    // Generate vertices
    for (int i = 0; i < numVertices - 1; ++i)
    {
        float theta = angle * i;
        vertices[i] = { { radius * cosf(theta), radius * sinf(theta) }, color };
    }

    // Initialize instance buffer
    for (int i = 0; i < numInstancesY; ++i)
    {
        auto pInstance = &instances[i * numInstancesX];

        for (int j = 0; j < numInstancesX; j++)
        {
            pInstance[j].pos = { -1.0f + j * (2.0f / numInstancesX) + radius, -1.0f + (i * offsetY) + (offsetY / 2.0f) };
            pInstance[j].color = { 0.5f * (j % 2), 0.5f * (j % 3), 0.5, 1.0 };
        }
    }

    // Fill indices
    int index = 0;
    for (int i = 0; i < 3 * (numVertices - 1); i += 3)
    {
        indices[i + 1] = uint16_t(index++);
        indices[i + 0] = uint16_t(index % numEdges);
        indices[i + 2] = uint16_t(numVertices - 1);
    }

    m_drawCount = numInstancesY;

    m_constantBuffer.initAsConstantBuffer(m_device.Get(), "constantBuffer", (void*)&constantBufferData, sizeof(CB));

    m_drawIndexedArg.initAsIndirectBuffer(m_device.Get(), "uavArg", DrawIndexedIndirectStride * numInstancesY);

    m_drawIndirectTask.init(this,
                            { .device = m_device.Get(),
                              .indirectArgBuffer = m_drawIndexedArg.get(),
                              .constantBuffer = m_constantBuffer.get(),
                              .color = color,
                              .vertexPosAndColor = vertices,
                              .numVerts = sizeof(vertices) / sizeof(vertices[0]),
                              .instancePosAndColor = instances,
                              .numInstances = sizeof(instances) / sizeof(instances[0]),
                              .indicesData = indices,
                              .numIndices = sizeof(indices) / sizeof(indices[0]),
                              .primitiveTopology = PrimitiveTopology::eTriangleList,
                              .colorAttachmentFormat = m_colorAttachmentFormat,
                              .depthStencilFormat = m_depthStencilAttachmentFormat });

    beginTransfer();
    m_drawIndirectTask.uploadStagingBuffers(m_copyCommandList.Get());
    m_constantBuffer.uploadFromStagingBuffer(m_copyCommandList.Get());
    endTransfer();

    return ResultCode::eSuccess;
}

void TestDrawIndexedIndirect::recordCommandList(CommandList* commandList)
{
    commandList->begin();

    m_drawIndirectTask.dispatch(commandList, 1);
    m_drawIndirectTask.drawIndirect(commandList, m_drawIndexedArg.get(), m_drawCount);
    commandList->end();
}

UnitTest* create_test_draw_indexed_indirect(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDrawIndexedIndirect(backend, config);
}

TestDrawIndirect::TestDrawIndirect(GraphicsBackend graphicsBackend, Config config)
    : UnitTest(graphicsBackend, config, "test_draw_indirect")
{
}

ResultCode TestDrawIndirect::init()
{
    ResultCode resultCode = UnitTest::init();
    if (resultCode != ResultCode::eSuccess)
    {
        return resultCode;
    }

    // Create vertex buffer
    const int numInstancesX = 4;
    const int numInstancesY = 4;
    const float offsetY = 2.0f / numInstancesY;

    const int numEdges = 4;
    const int numVertices = numEdges * 3;

    const float radius = 2.0f / (numInstancesX * 2.0f);
    const float4 color = { 0.5f, 0.0f, 0.5f, 1.0f };
    const CB constantBufferData = { .numVertices = numVertices, .numInstances = numInstancesX };

    VertexPosColor vertices[numVertices] = { { { 0.0f, 0.0f }, {} },  { { 0.25f, 0.0f }, {} },  { { 0.0f, 0.25f }, {} }, { { 0.0f, 0.0f }, {} },
                                             { { 0.25f, 0.0f }, {} }, { { 0.0f, -0.25f }, {} }, { { 0.0f, 0.0f }, {} },  { { -0.25f, 0.0f }, {} },
                                             { { 0.0f, -0.25 }, {} }, { { 0.0f, 0.0f }, {} },   { { 0.0f, 0.25f }, {} }, { { -0.25f, 0.0f }, {} } };
    InstancePosColor instances[numInstancesY * numInstancesX];

    // Generate vertices

    // Initialize instance buffer
    for (int i = 0; i < numInstancesY; ++i)
    {
        auto pInstance = &instances[i * numInstancesX];

        for (int j = 0; j < numInstancesX; j++)
        {
            pInstance[j].pos = { -1.0f + j * (2.0f / numInstancesX) + radius, -1.0f + (i * offsetY) + (offsetY / 2.0f) };
            pInstance[j].color = { 0.5f * (j % 2), 0.5f * (j % 3), 0.5, 1.0 };
        }
    }

    m_drawCount = numInstancesY;

    m_constantBuffer.initAsConstantBuffer(m_device.Get(), "constantBuffer", (void*)&constantBufferData, sizeof(CB));

    m_drawArg.initAsIndirectBuffer(m_device.Get(), "uavArg", DrawIndirectStride * numInstancesY);

    m_drawIndirectTask.init(this,
                            { .device = m_device.Get(),
                              .indirectArgBuffer = m_drawArg.get(),
                              .constantBuffer = m_constantBuffer.get(),
                              .color = color,
                              .vertexPosAndColor = vertices,
                              .numVerts = sizeof(vertices) / sizeof(vertices[0]),
                              .instancePosAndColor = instances,
                              .numInstances = sizeof(instances) / sizeof(instances[0]),
                              .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                              .colorAttachmentFormat = m_colorAttachmentFormat,
                              .depthStencilFormat = m_depthStencilAttachmentFormat });

    beginTransfer();
    m_drawIndirectTask.uploadStagingBuffers(m_copyCommandList.Get());
    m_constantBuffer.uploadFromStagingBuffer(m_copyCommandList.Get());
    endTransfer();

    return ResultCode::eSuccess;
}

void TestDrawIndirect::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    m_drawIndirectTask.dispatch(commandList, 1);
    m_drawIndirectTask.drawIndirect(commandList, m_drawArg.get(), m_drawCount);
    commandList->end();
}

UnitTest* create_test_draw_indirect(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDrawIndirect(backend, config);
}
