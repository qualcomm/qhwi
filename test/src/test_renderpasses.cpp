//
//
//  test_renderpasses.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear

#include "test_renderpasses.h"
#ifdef QHWI_BACKEND_VULKAN
#    include "qhwi/qhwi_vk.h"
#endif
using namespace test_util;

void FirstSubpassTask::init(TestSubpasses* testSubpasses,
                            qhwi::Device* device,
                            float4 color,
                            float2* posData,
                            uint32_t numVerts,
                            uint16_t* indicesData,
                            uint32_t numIndices,
                            qhwi::PrimitiveTopology primitiveTopology)
{
    Task::init(device);
    m_testSubpasses = testSubpasses;
    m_color = color;
    m_primitiveTopology = primitiveTopology;

    CbData cbData = { .color = m_color };

    Quad quad;
    if (!posData)
    {
        quad.init();
        posData = quad.pos;
        numVerts = quad.NumVerts;
    }

    m_numVerts = numVerts;
    m_numIndices = numIndices;

    m_cb.initAsConstantBuffer(m_device, "cb", &cbData, sizeof(CbData));
    m_bPos.initAsVertexBuffer(m_device, "pos", posData, numVerts * sizeof(float2));
    if (m_numIndices)
    {
        m_bIndices.initAsIndexBuffer(m_device, "indices", indicesData, numIndices * sizeof(uint16_t));
    }
    initDescriptorSetLayouts();
    initDescriptorSets();
    initPipelineLayout();
    initPipeline();
}

void FirstSubpassTask::uploadStagingBuffers(qhwi::CommandList* cmdList)
{
    m_cb.uploadFromStagingBuffer(cmdList);
    m_bPos.uploadFromStagingBuffer(cmdList);
    if (m_numIndices)
    {
        m_bIndices.uploadFromStagingBuffer(cmdList);
    }
}

void FirstSubpassTask::draw(qhwi::CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline);
    cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), qhwi::IndexFormat::eUint16);
    }
    cmdList->bindGraphicsDescriptorSet(m_descriptorSets[0]);
    if (hasIndices)
    {
        cmdList->drawIndexed(m_numIndices, 1);
    }
    else
    {
        cmdList->draw(m_numVerts);
    }
}

void FirstSubpassTask::initDescriptorSetLayouts()
{
    m_descriptorSetLayouts[0] = m_device->createDescriptorSetLayout(qhwi::DescriptorSetLayoutDesc().addConstantBuffer(0, qhwi::ShaderStage_Vertex));
}

void FirstSubpassTask::initDescriptorSets()
{
    m_descriptorSets[0] = m_device->createDescriptorSet(qhwi::DescriptorSetDesc().setLayout(m_descriptorSetLayouts[0]).addConstantBuffer(0, m_cb.get()));
}

void FirstSubpassTask::initPipelineLayout()
{
    m_graphicsPipelineLayout = m_device->createPipelineLayout(qhwi::PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));
}

void FirstSubpassTask::initShaders(Shader1& vs, Shader1& ps)
{
    initVertexShader(m_device, "testRenderPasses_0", vs);
    initPixelShader(m_device, "testRenderPasses_0", ps);
}

void FirstSubpassTask::initPipelineDesc(const Shader1& vs, const Shader1& ps, qhwi::GraphicsPipelineDesc& gpDesc)
{
    gpDesc = qhwi::GraphicsPipelineDesc(
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
                                                                        .binding = B_POS,
                                                                        .format = qhwi::Format::eRg32Float,
                                                                        .offset = 0,
                                                                        .stride = sizeof(float2),
                                                                        .inputRate = qhwi::VertexInputRate::eVertex } } },
          .viewportState = {},
          .scissorState = {},
          .layout = m_graphicsPipelineLayout,
          .primitiveTopology = m_primitiveTopology,
          .primitiveRestartValue = {},
          .colorAttachmentFormats = { m_testSubpasses->getColorAttachmentFormat(), qhwi::Format::eR8Unorm, qhwi::Format::eR8Unorm, qhwi::Format::eR8Unorm, qhwi::Format::eR8Unorm },
          .depthStencilFormat = {},
          .meshShaderParams = {},
          .multiViewState = {},
          .colorAttachmentRemapping = { qhwi::AttachmentUnused, 0, 1, 2, 3 },
          .name = nullptr });
}

void FirstSubpassTask::initPipeline()
{
    Shader1 vs, ps;
    initShaders(vs, ps);
    qhwi::GraphicsPipelineDesc gpDesc;
    initPipelineDesc(vs, ps, gpDesc);
    m_graphicsPipeline = m_device->createGraphicsPipeline(gpDesc);
}

void SecondSubpassTask::init(TestSubpasses* testSubpasses,
                             qhwi::Device* device,
                             Texture1* intermediateTextures,
                             float2* posData,
                             uint32_t numVerts,
                             uint16_t* indicesData,
                             uint32_t numIndices,
                             float2* uvData,
                             qhwi::Sampler::Description samplerDesc)
{
    Task::init(device);
    m_testSubpasses = testSubpasses;
    m_intermediateTextures = intermediateTextures;
    m_numVerts = numVerts;
    m_numIndices = numIndices;

    Quad quad;
    if (!posData)
    {
        quad.init();
        posData = quad.pos;
        m_numVerts = quad.NumVerts;
    }
    m_bPos.initAsVertexBuffer(m_device, "pos", posData, m_numVerts * sizeof(float2));
    m_sampler = m_device->createSampler(std::move(samplerDesc));

    initDescriptorSetLayouts();
    initDescriptorSets();
    initPipelineLayout();
    initPipeline();
}

void SecondSubpassTask::draw(qhwi::CommandList* cmdList)
{
    bool hasIndices = (m_numIndices != 0);
    cmdList->bindGraphicsPipeline(m_graphicsPipeline);
    cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
    if (hasIndices)
    {
        cmdList->bindIndexBuffer(m_bIndices.get(), qhwi::IndexFormat::eUint16);
    }
    cmdList->bindGraphicsDescriptorSet(m_descriptorSets[0]);
    if (hasIndices)
    {
        cmdList->drawIndexed(m_numIndices, 1);
    }
    else
    {
        cmdList->draw(m_numVerts);
    }
}

void SecondSubpassTask::uploadStagingBuffers(qhwi::CommandList* cmdList)
{
    if (!m_isUma)
    {
        m_bPos.uploadFromStagingBuffer(cmdList);
    }
}

void SecondSubpassTask::initDescriptorSetLayouts()
{
    m_descriptorSetLayouts[0] = m_device->createDescriptorSetLayout(qhwi::DescriptorSetLayoutDesc()
                                                                        .addInputAttachment(0, qhwi::ShaderStage_Pixel)
                                                                        .addInputAttachment(1, qhwi::ShaderStage_Pixel)
                                                                        .addInputAttachment(2, qhwi::ShaderStage_Pixel)
                                                                        .addInputAttachment(3, qhwi::ShaderStage_Pixel));
}

void SecondSubpassTask::initDescriptorSets()
{
    m_descriptorSets[0] = m_device->createDescriptorSet(qhwi::DescriptorSetDesc()
                                                            .setLayout(m_descriptorSetLayouts[0])
                                                            .addInputAttachment(0, m_intermediateTextures[0].get())
                                                            .addInputAttachment(1, m_intermediateTextures[1].get())
                                                            .addInputAttachment(2, m_intermediateTextures[2].get())
                                                            .addInputAttachment(3, m_intermediateTextures[3].get()));
}

void SecondSubpassTask::initPipelineLayout()
{
    m_graphicsPipelineLayout = m_device->createPipelineLayout(qhwi::PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));
}

void SecondSubpassTask::initShaders(Shader1& vs, Shader1& ps)
{
    initVertexShader(m_device, "testRenderPasses_1", vs);
    initPixelShader(m_device, "testRenderPasses_1", ps);
}

void SecondSubpassTask::initPipelineDesc(const Shader1& vs, const Shader1& ps, qhwi::GraphicsPipelineDesc& gpDesc)
{
    gpDesc = qhwi::GraphicsPipelineDesc(
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
                    .binding = B_POS,
                    .format = qhwi::Format::eRg32Float,
                    .offset = 0,
                    .stride = sizeof(float2),
                    .inputRate = qhwi::VertexInputRate::eVertex
                }
            },
        },
        .viewportState = {},
        .scissorState = {},
        .layout = m_graphicsPipelineLayout,
        .primitiveTopology = qhwi::PrimitiveTopology::eTriangleStrip,
        .primitiveRestartValue = {},
        .colorAttachmentFormats =
        {
            m_testSubpasses->getColorAttachmentFormat(),
            qhwi::Format::eR8Unorm,
            qhwi::Format::eR8Unorm,
            qhwi::Format::eR8Unorm,
            qhwi::Format::eR8Unorm
        },
        .depthStencilFormat = {},
        .meshShaderParams = {},
        .multiViewState = {},
        .colorAttachmentRemapping = { 0, qhwi::AttachmentUnused, qhwi::AttachmentUnused, qhwi::AttachmentUnused, qhwi::AttachmentUnused },
        .name = nullptr
    });
}

void SecondSubpassTask::initPipeline()
{
    Shader1 vs, ps;
    initShaders(vs, ps);
    qhwi::GraphicsPipelineDesc gpDesc;
    initPipelineDesc(vs, ps, gpDesc);
    m_graphicsPipeline = m_device->createGraphicsPipeline(gpDesc);
}

qhwi::ResultCode TestSubpasses::init()
{
    qhwi::ResultCode resultCode = UnitTest::init();
    if (resultCode != qhwi::ResultCode::eSuccess)
    {
        return resultCode;
    }
    if (m_device->getGraphicsBackend() == qhwi::GraphicsBackend::eVulkan)
    {
#ifdef QHWI_BACKEND_VULKAN
        auto vkDevice = static_cast<qhwi::VkDevice*>(m_device.Get());
        if (!vkDevice->supportsDynamicRenderingLocalRead())
        {
            return qhwi::ResultCode::eUnsupported;
        }
#endif
    }
    initTextures();
    initRenderPass();
    initFramebuffers();
    initSubpassTasks();
    m_firstSubpassTask->init(this, m_device.Get(), { 1.0f, 0.8f, 0.6f, 1.0f });
    m_secondSubpassTask->init(this, m_device.Get(), m_texture);
    beginTransfer();
    m_firstSubpassTask->uploadStagingBuffers(m_copyCommandList);
    m_secondSubpassTask->uploadStagingBuffers(m_copyCommandList);
    endTransfer();
    return qhwi::ResultCode::eSuccess;
}

void TestSubpasses::initTextures()
{
    for (uint32_t j = 0; j < NumIntermediateAttachments; j++)
    {
        std::string name = "intermediate_attachment_" + std::to_string(j);
        m_texture[j].init(m_device.Get(),
                          { .heapFlags = {},
                            .w = m_colorAttachmentWidth,
                            .h = m_colorAttachmentHeight,
                            .d = 1,
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .numPlanes = 1,
                            .numSamples = qhwi::SampleCount::e1,
                            .format = qhwi::Format::eR8Unorm,
                            .rSwizzle = {},
                            .gSwizzle = qhwi::ComponentSwizzle::eG,
                            .bSwizzle = qhwi::ComponentSwizzle::eB,
                            .aSwizzle = qhwi::ComponentSwizzle::eA,
                            .usage = qhwi::TextureUsageMask(uint8_t(qhwi::TextureUsage_ColorAttachment) | uint8_t(qhwi::TextureUsage_InputAttachment)),
                            .type = qhwi::TextureType::e2D,
                            .name = name.c_str(),
                            .automaticBarriers = false });
    }
}

void TestSubpasses::beginRenderPass(qhwi::CommandList* cmdList, qhwi::AttachmentLoadOp)
{
    const auto& desc = m_texture[0].get()->desc();
    uint32_t w = desc->w, h = desc->h;

    for (uint32_t j = 0; j < NumIntermediateAttachments; j++)
    {
        cmdList->addTextureBarrier({ .texture = m_texture[j].get(),
                                     .src = (m_frame == 0) ? qhwi::TextureState_NoAccess : qhwi::TextureState_PixelShader_Sample_LocalRead,
                                     .dst = qhwi::TextureState_ColorAttachment_LocalRead });
    }

    // need to specify all the attachments used by the whole renderpass, not just the current subpass
    std::array<qhwi::ColorAttachmentDescription, NumIntermediateAttachments + 1> caDescs;
    auto caDescsIt = caDescs.begin();
    qhwi::Texture* colorAttachment = (m_config.onscreen) ? m_swapchain.get()->getCurrentImage() : m_offscreenSurface.get();
    (*caDescsIt).setTexture(colorAttachment).setLoadOp(qhwi::AttachmentLoadOp::eClear).setStoreOp(qhwi::AttachmentStoreOp::eStore);
    ++caDescsIt;
    for (uint32_t j = 0; j < NumIntermediateAttachments; j++)
    {
        (*caDescsIt).setTexture(m_texture[j].get()).setLoadOp(qhwi::AttachmentLoadOp::eClear).setStoreOp(qhwi::AttachmentStoreOp::ePreserveLocalSrv);
        ++caDescsIt;
    }
    std::array<uint32_t, NumIntermediateAttachments + 1> colorAttachmentRemapping = { qhwi::AttachmentUnused, 0, 1, 2, 3 };
    cmdList->beginRenderPass((uint32_t)caDescs.size(), caDescs.data(), nullptr, colorAttachmentRemapping.data());
    cmdList->setViewport({ .w = float(w), .h = float(h) });
    cmdList->setScissor({ .w = w, .h = h });
}

void TestSubpasses::nextSubpass(qhwi::CommandList* cmdList)
{
    endRenderPass(cmdList);

    const auto& desc = m_texture[0].get()->desc();
    uint32_t w = desc->w, h = desc->h;
    std::array<qhwi::ColorAttachmentDescription, 1 + NumIntermediateAttachments> caDescs;
    auto caDescPtr = &caDescs[0];
    auto outputTexture = m_config.onscreen ? m_swapchain->getCurrentImage() : m_offscreenSurface.get();
    (*caDescPtr).setLoadOp(qhwi::AttachmentLoadOp::eClear).setStoreOp(qhwi::AttachmentStoreOp::eStore).setTexture(outputTexture);
    ++caDescPtr;
    for (uint32_t j = 0; j < NumIntermediateAttachments; j++)
    {
        (*caDescPtr).setLoadOp(qhwi::AttachmentLoadOp::ePreserveLocalSrv).setStoreOp(qhwi::AttachmentStoreOp::eDontCare).setTexture(m_texture[j].get());
        cmdList->addTextureBarrier({ .texture = m_texture[j].get(), .src = qhwi::TextureState_ColorAttachment_LocalRead, .dst = qhwi::TextureState_PixelShader_Sample_LocalRead });
        ++caDescPtr;
    }

    std::array<uint32_t, 5> colorAttachmentRemapping = { 0, qhwi::AttachmentUnused, qhwi::AttachmentUnused, qhwi::AttachmentUnused, qhwi::AttachmentUnused };

    cmdList->beginRenderPass((uint32_t)caDescs.size(), caDescs.data(), nullptr, colorAttachmentRemapping.data());
    cmdList->setViewport({ .w = float(w), .h = float(h) });
    cmdList->setScissor({ .w = w, .h = h });
}

void TestSubpasses::recordCommandList(qhwi::CommandList* commandList)
{
    commandList->begin();
    beginRenderPass(commandList);
    m_firstSubpassTask->draw(commandList);
    nextSubpass(commandList);
    m_secondSubpassTask->draw(commandList);
    endRenderPass(commandList);
    if (m_config.onscreen)
    {
        commandList->addTextureBarrier({ .texture = m_swapchain->getCurrentImage(), .src = qhwi::TextureState_ColorAttachment, .dst = qhwi::TextureState_Present });
    }
    commandList->end();
    m_frame++;
}

UnitTest* create_test_renderpass(qhwi::GraphicsBackend backend, UnitTest::Config config)
{
    config.clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    return new TestRenderPass(backend, config);
}

UnitTest* create_test_subpasses(qhwi::GraphicsBackend backend, UnitTest::Config config)
{
    return new TestSubpasses(backend, config);
}
