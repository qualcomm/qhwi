//
//
//  test_multiple_render_targets.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_multiple_render_targets.h"

test_util::Texture1& TestMultipleRenderTargets::getRenderpassTexture(int index)
{
    return m_firstRenderPassTexture[index];
}

qhwi::ResultCode TestMultipleRenderTargets::init()
{
    ResultCode resultCode = UnitTest::init();
    if (resultCode != ResultCode::eSuccess)
    {
        return resultCode;
    }

    // set up first firstRenderpassTask
    const int NVertices = 4;

    float2 meshVertices[NVertices] = { { -0.4f, -0.4f }, { -0.4f, +0.4f }, { +0.4f, -0.4f }, { +0.4f, +0.4f } };
    float2 meshVerticesFullScreen[NVertices] = { { -1.0f, -1.0f }, { -1.0f, +1.0f }, { +1.0f, -1.0f }, { +1.0f, +1.0f } };

    float2 meshTexCoords[NVertices] = {
        { 0.0f, 1.0f },
        { 0.0f, 0.0f },
        { 1.0f, 1.0f },
        { 1.0f, 0.0f },
    };

    float4 color = { 1.0f, 0.5f, 0.7f, 1.0f };

    initTextures();

    m_firstRenderpassTask.init(m_device.Get(), color, meshVertices, NVertices, qhwi::PrimitiveTopology::eTriangleStrip, qhwi::Format::eR8Unorm, qhwi::Format::eUnknown);

    qhwi::SamplerDesc samplerDesc = {
        .minFilter = {},
        .magFilter = {},
        .mipFilter = {},
        .addressModeU = SamplerAddressMode::eClampToEdge,
        .addressModeV = SamplerAddressMode::eClampToEdge,
        .addressModeW = SamplerAddressMode::eClampToEdge,
        .mipLodBias = {},
        .minLod = {},
        .maxLod = {},
        .maxAnisotropy = {},
        .flags = {},
        .reductionMode = {},
        .compareOp = {},
    };
    m_secondRenderpassTask.init(m_device.Get(),
                                NTextureAttachments,
                                m_firstRenderPassTexture,
                                color,
                                meshVerticesFullScreen,
                                NVertices,
                                meshTexCoords,
                                samplerDesc,
                                qhwi::PrimitiveTopology::eTriangleStrip,
                                m_colorAttachmentFormat,
                                qhwi::Format::eUnknown);

    beginTransfer();
    m_firstRenderpassTask.uploadStagingBuffers(m_copyCommandList.Get());
    m_secondRenderpassTask.uploadStagingBuffers(m_copyCommandList.Get());
    endTransfer();

    return qhwi::ResultCode::eSuccess;
}

void TestMultipleRenderTargets::recordCommandList(qhwi::CommandList* commandList)
{
    commandList->begin();

    qhwi::ColorAttachmentDescription colorAttachmentDescs[3]{};
    for (int j = 0; j < 3; j++)
    {
        auto& colorAttachmentDesc = colorAttachmentDescs[j];
        colorAttachmentDesc.loadOp = AttachmentLoadOp::eClear;
        colorAttachmentDesc.storeOp = AttachmentStoreOp::eStore;
        colorAttachmentDesc.clearValue.rgba = m_config.clearColor;
        colorAttachmentDesc.setTexture(m_firstRenderPassTexture[j].get());
    }

    commandList->beginRenderPass(3, colorAttachmentDescs, nullptr);
    commandList->setViewport({ .w = float(m_textureWidth), .h = float(m_textureHeight) });
    commandList->setScissor({ .w = m_textureWidth, .h = m_textureHeight });
    m_firstRenderpassTask.draw(commandList);
    commandList->endRenderPass();

    beginRenderPass(commandList);
    m_secondRenderpassTask.draw(commandList);
    endRenderPass(commandList);
    commandList->end();
}

void TestMultipleRenderTargets::initTextures()
{
    for (size_t i = 0; i < NTextureAttachments; i++)
    {
        std::string name = "renderpass1_attachment_" + std::to_string(i);
        m_firstRenderPassTexture[i].init(m_device.Get(),
                                         { .heapFlags = {},
                                           .w = m_textureWidth,
                                           .h = m_textureHeight,
                                           .d = 1,
                                           .mipLevels = {},
                                           .arrayLayers = 1,
                                           .numPlanes = 1,
                                           .numSamples = SampleCount::e1,
                                           .format = Format::eR8Unorm,
                                           .rSwizzle = {},
                                           .gSwizzle = ComponentSwizzle::eG,
                                           .bSwizzle = ComponentSwizzle::eB,
                                           .aSwizzle = ComponentSwizzle::eA,
                                           .usage = TextureUsage::TextureUsage_ColorAttachment | TextureUsage::TextureUsage_Sampled,
                                           .type = TextureType::e2D,
                                           .name = name.c_str(),
                                           .automaticBarriers = true });
    }
}

void FirstRenderpassTask::init(Device* device,
                               float4 color,
                               float2* posData,
                               uint32_t numVerts,
                               PrimitiveTopology primitiveTopology,
                               Format colorAttachmentFormat,
                               Format depthStencilFormat)
{
    Task::init(device);

    m_numVerts = numVerts;
    m_primitiveTopology = primitiveTopology;
    m_color = color;

    CbData cbData = { m_color };

    m_constantBuffer.initAsConstantBuffer(m_device, "cb", &cbData, sizeof(CbData));
    m_bPos.initAsVertexBuffer(m_device, "pos", posData, numVerts * sizeof(float2));

    initDescriptorSetLayouts();
    initDescriptorSets();
    initPipeline(primitiveTopology, colorAttachmentFormat, depthStencilFormat);
}

void FirstRenderpassTask::initDescriptorSetLayouts()
{
    m_descriptorSetLayout = m_device->create(DescriptorSetLayoutDesc().addConstantBuffer(0, ShaderStage_Vertex));
}

void FirstRenderpassTask::initDescriptorSets()
{
    m_descriptorSet = m_device->createDescriptorSet(DescriptorSetDesc(m_descriptorSetLayout).addConstantBuffer(0, m_constantBuffer.buffer));
}

void FirstRenderpassTask::initPipeline(PrimitiveTopology primitiveTopology, Format colorAttachmentFormat, Format depthStencilFormat)
{
    m_graphicsPipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayout));

    Shader1 vs, ps;
    initVertexShader(m_device, "testMultipleRenderTargets1", vs);
    initPixelShader(m_device, "testMultipleRenderTargets1", ps);
    m_graphicsPipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayout));
    m_graphicsPipeline = m_device->createGraphicsPipeline(
        {
            .vertexShader = vs.shader,
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
            .vertexInputState =
            {
                .vertexInputAttributeDescriptions =
                {
                    {
                        .semanticName = "POSITION",
                        .semanticIndex = 0,
                        .binding = 0,
                        .format = Format::eRg32Float,
                        .offset = {},
                        .stride = sizeof(float2),
                        .inputRate = {}
                    }
                }
            },
            .viewportState = {},
            .scissorState = {},
            .layout = m_graphicsPipelineLayout,
            .primitiveTopology = primitiveTopology,
            .primitiveRestartValue = {},
            .colorAttachmentFormats =
            {
                colorAttachmentFormat,
                colorAttachmentFormat,
                colorAttachmentFormat,
            },
            .depthStencilFormat = depthStencilFormat,
            .meshShaderParams = {},
            .multiViewState = {},
            .colorAttachmentRemapping = {},
            .name = {},
        });
}

void FirstRenderpassTask::uploadStagingBuffers(CommandList* cmdList)
{
    m_bPos.uploadFromStagingBuffer(cmdList);
    m_constantBuffer.uploadFromStagingBuffer(cmdList);
}

void FirstRenderpassTask::draw(CommandList* cmdList)
{

    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_bPos.get(), 0, 0, sizeof(float2));
    cmdList->bindGraphicsDescriptorSet(m_descriptorSet.Get());
    cmdList->draw(m_numVerts);
}

qhwi::Format FirstRenderpassTask::getColorAttachmetFormat()
{
    return m_colorAttachmentFormat;
}

void SecondRenderpassTask::init(Device* device,
                                int numTextures,
                                test_util::Texture1* intermediateTextures,
                                float4 color,
                                float2* posData,
                                uint32_t numVerts,
                                test_util::float2* uvData,
                                qhwi::SamplerDesc samplerDesc,
                                PrimitiveTopology primitiveTopology,
                                Format colorAttachmentFormat,
                                Format depthStencilFormat)
{
    Task::init(device);

    m_numTextures = numTextures;
    m_inputTextures = intermediateTextures;
    m_primitiveTopology = primitiveTopology;
    m_numVerts = numVerts;

    m_bPos.initAsVertexBuffer(m_device, "pos", posData, numVerts * sizeof(float2));
    m_bTexCords.initAsVertexBuffer(m_device, "uvw", uvData, numVerts * sizeof(float2));

    m_sampler = m_device->createSampler(samplerDesc);

    initDescriptorSetsAndLayouts();
    initPipeline(primitiveTopology, colorAttachmentFormat, depthStencilFormat);
}

void SecondRenderpassTask::initDescriptorSetsAndLayouts()
{
    m_device->createDescriptorSetAndLayout(
        DescriptorSetDesc().addTexture(0, m_inputTextures[0].texture).addTexture(1, m_inputTextures[1].texture).addTexture(2, m_inputTextures[2].texture).addSampler(0, m_sampler),
        ShaderStage_Pixel,
        0,
        m_descriptorSetLayout,
        m_descriptorSet);
}

void SecondRenderpassTask::initPipeline(PrimitiveTopology primitiveTopology, Format colorAttachmentFormat, Format depthStencilFormat)
{
    m_graphicsPipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayout));
    Shader1 vs, ps;
    initVertexShader(m_device, "testMultipleRenderTargets2", vs);
    initPixelShader(m_device, "testMultipleRenderTargets2", ps);

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
                                                                                                                          .binding = 0,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(float2),
                                                                                                                          .inputRate = {} },
                                                                                                                        { .semanticName = "TEXCOORD",
                                                                                                                          .semanticIndex = 0,
                                                                                                                          .binding = 1,
                                                                                                                          .format = Format::eRg32Float,
                                                                                                                          .offset = {},
                                                                                                                          .stride = sizeof(float2),
                                                                                                                          .inputRate = {} } } },
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

void SecondRenderpassTask::uploadStagingBuffers(CommandList* cmdList)
{
    m_bPos.uploadFromStagingBuffer(cmdList);
    m_bTexCords.uploadFromStagingBuffer(cmdList);
}

void SecondRenderpassTask::draw(CommandList* cmdList)
{

    cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
    cmdList->bindVertexBuffer(m_bPos.get(), 0, 0, sizeof(float2));
    cmdList->bindVertexBuffer(m_bTexCords.get(), 1, 0, sizeof(float2));
    cmdList->bindDescriptorSet(m_descriptorSet.Get(), PipelineBindPoint::eGraphics, m_graphicsPipelineLayout.Get());
    cmdList->draw(m_numVerts);
}

UnitTest* create_test_multiple_render_targets(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestMultipleRenderTargets(backend, config);
}
