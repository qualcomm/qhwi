//
//
//  qcom_tile_shading.cpp
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <chrono>
#include <thread>

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <qhwi/qhwi_android_window.h>
#endif

#include "qcom_tile_shading.h"
#include <qhwi/qhwi_util.h>
#include <qre/text/textutil.h>

// TODO: enable HLSL when dxc supports QCOM_Tile_Shading
#define SHADER_TYPE "glsl"

QCOMTileShadingExample::~QCOMTileShadingExample()
{
    shutdown();
}

ResultCode QCOMTileShadingExample::init(int argc, const char* const* argv)
{

    // High level description:
    //
    // attachments: { 0, 1 }
    // for each triangle (3 total):
    // fragment shader: render to 0
    // compute shader: sample 0 apply blur and output to 1
    //
    // fragment shader: render triangle to 1
    // compute shader (debugging): sample buffer and output to SSBO.  TODO: use tile memory heap for buffer allocation

    cxxopts::Options opts("QCOMTileShadingExample", "Run QCOM tile shading example");
    Example::initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    Example::parse(argc, argv, opts, parseResult, m_config);
    m_config.autoBarrier = false;
    std::array swapchainFormatCandidates = { qhwi::Format::eRgba8Unorm };
    m_config.swapchainFormatCandidates = swapchainFormatCandidates;
    m_config.swapchainImageUsage |= TextureUsage_Storage;
    m_config.apiDump = false;
    Example::init("QCOMTileShadingExample", &m_config);

#pragma region create vertex buffer
    const float4 pos[] = { { -0.9f, -0.9f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.9f, -0.9f, 1.0f, 1.0f },  { 0.9f, -0.9f, 1.0f, 1.0f },
                           { 0.0f, 0.0f, 1.0f, 1.0f },   { 0.9f, 0.9f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },   { -0.9f, 0.9f, 1.0f, 1.0f },
                           { 0.9f, 0.9f, 1.0f, 1.0f },   { 0.0f, 0.0f, 1.0f, 1.0f }, { -0.9f, -0.9f, 1.0f, 1.0f }, { -0.9f, 0.9f, 1.0f, 1.0f } };
    const float2 texcoords[] = {
        { 0.0f, 0.0f },
        { 0.0f, 1.0f },
        { 1.0f, 0.0f },
    };
    m_pos.init(m_device.Get(), BufferDesc().setSize(sizeof(pos)).setUsage(BufferUsage_VertexBuffer).setHeapType(HeapType::eShared), (void*)pos);
    m_texCoords.init(m_device.Get(), BufferDesc().setSize(sizeof(texcoords)).setUsage(BufferUsage_VertexBuffer).setHeapType(HeapType::eShared), (void*)texcoords);
#pragma endregion

#pragma region create texture and sampler
    uint32_t texData[s_TextureWidth * s_TextureHeight];
    uint8_t* pTextureData = (uint8_t*)texData;
    for (uint32_t i = 0; i < s_TextureHeight; i++)
    {
        uint8_t v = (i & 4) ? 0x00 : 0xFF;
        for (uint32_t j = 0; j < s_TextureWidth; j++)
        {
            *pTextureData++ = (j & 4) ? v : ~v;
            *pTextureData++ = (j & 4) ? v : ~v;
            *pTextureData++ = (j & 4) ? v : ~v;
            *pTextureData++ = 0xFF;
        }
    }
    m_texImage.init(m_device.Get(), TextureDesc().setWidth(s_TextureWidth).setHeight(s_TextureHeight), texData);

    m_sampler = m_device->createSampler(SamplerDesc().setMinFilter(Filter::eNearest).setMagFilter(Filter::eNearest));

    m_offscreenTexture.init(m_device.Get(),
                            TextureDesc()
                                .setWidth(s_OffscreenTextureWidth)
                                .setHeight(s_OffscreenTextureHeight)
                                .setType(TextureType::e2DArray)
                                .setFormat(Format::eRgba8Unorm)
                                .setArrayLayers(2)
                                .setUsage(TextureUsage_ColorAttachment | TextureUsage_Storage));
#pragma endregion

    m_fence = m_device->createFence();

#pragma region create compute buffer, transient buffer
    m_computeBuffer.initAsUnorderedAccessBuffer(m_device.Get(), "computeBuffer", nullptr, s_TileTransientBufferSize);
    qhwi::BufferDesc tileTransientBufferDesc = { .heapType = qhwi::HeapType::eDefault,
                                                 .usage = qhwi::BufferUsage_StorageBuffer,
                                                 .size = s_TileTransientBufferSize,
                                                 .name = "tileTransientBuffer" };
    m_transientBuffer.init(m_device.Get(), tileTransientBufferDesc);

#pragma endregion

#pragma region create desc set and layout
    BindingSetOffsets bindingSetOffsets = { 0, 0, 0, 0 };
    m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addTexture(0, m_texImage.get()).addSampler(2, m_sampler),
                                           qhwi::ShaderStage_Pixel,
                                           0,
                                           m_descSetLayout,
                                           m_descSet,
                                           bindingSetOffsets);
#pragma endregion

#pragma region create cs desc set and layout
    qhwi::DescriptorSetLayoutDesc csDescSetLayoutDesc(DescriptorSetLayoutDesc()
                                                          .clearBindingSetOffsets()
                                                          .addStorageBuffer(0, qhwi::ShaderStage_Compute)
                                                          .addStorageImage(1, qhwi::ShaderStage_Compute)
                                                          .addStorageBuffer(2, qhwi::ShaderStage_Compute)
                                                          .addStorageImage(3, qhwi::ShaderStage_Compute));
    m_csDescSetLayout = m_device->createDescriptorSetLayout(csDescSetLayoutDesc);

    qhwi::DescriptorSetDesc csDescSetDesc(DescriptorSetDesc()
                                              .setLayout(m_csDescSetLayout)
                                              .addStorageBuffer(0, m_computeBuffer.get(), sizeof(float4))
                                              .addStorageImage(1, m_offscreenTexture.texture, SubresourceRange({ .arraySlice = 0 }))
                                              .addStorageBuffer(2, m_transientBuffer.get(), s_TileTransientBufferSize)
                                              .addStorageImage(3, m_offscreenTexture.texture, SubresourceRange({ .arraySlice = 1 })));
    m_csDescSet = m_device->createDescriptorSet(csDescSetDesc);
#pragma endregion

#pragma region create pipeline layouts
    m_pipelineLayout = m_device->createPipelineLayout(PipelineLayoutDesc().addDescriptorSetLayout(m_descSetLayout));

    m_csPipelineLayout = m_device->createPipelineLayout(PipelineLayoutDesc().addDescriptorSetLayout(m_csDescSetLayout));

    m_gfxPipelineLayout = m_device->createPipelineLayout(PipelineLayoutDesc().addDescriptorSetLayout(m_descSetLayout));
#pragma endregion

#pragma region create pipelines
    for (uint32_t j = 0; j < s_NumGfxPipelines; j++)
    {
        FileData vsData, fsData;
        char s0[128];
        qre::PrintfSafe(s0, "shaders/vert%d." SHADER_TYPE ".spv", j + 1);
        FileUtil::readAndroidAsset(s0, vsData);
        qre::PrintfSafe(s0, "shaders/frag%d." SHADER_TYPE ".spv", j + 1);
        FileUtil::readAndroidAsset(s0, fsData);

        m_vs[j] = m_device->createShader(ShaderDesc().setShaderType(ShaderType::eVertex).setEntryPointName("main"), vsData.data, vsData.size);
        m_ps[j] = m_device->createShader(ShaderDesc().setShaderType(ShaderType::ePixel).setEntryPointName("main"), fsData.data, fsData.size);
    }

    FileData csData1, csData2;
    FileUtil::readAndroidAsset("shaders/compute1." SHADER_TYPE ".spv", csData1);
    FileUtil::readAndroidAsset("shaders/compute2." SHADER_TYPE ".spv", csData2);

    m_cs1 = m_device->createShader(ShaderDesc().setShaderType(ShaderType::eCompute).setEntryPointName("main"), csData1.data, csData1.size);
    m_cs2 = m_device->createShader(ShaderDesc().setShaderType(ShaderType::eCompute).setEntryPointName("main"), csData2.data, csData2.size);

    // Compute pipeline
    m_cs1Pipeline = m_device->createComputePipeline(ComputePipelineDesc().setLayout(m_csPipelineLayout).setComputeShader(m_cs1));

    m_cs2Pipeline = m_device->createComputePipeline(ComputePipelineDesc().setLayout(m_csPipelineLayout).setComputeShader(m_cs2));

    qhwi::VertexInputState vertexInputState = {
        .vertexInputAttributeDescriptions = { { .semanticName = "POSITION", .semanticIndex = 0, .binding = 0, .format = qhwi::Format::eRgba32Float, .stride = sizeof(float4) },
                                              { .semanticName = "TEXCOORD", .semanticIndex = 0, .binding = 1, .format = qhwi::Format::eRg32Float, .stride = sizeof(float2) } }
    };

    for (uint32_t j = 0; j < s_NumGfxPipelines; j++)
    {
        auto gfxPipelineDesc = GraphicsPipelineDesc()
                                   .setLayout(m_gfxPipelineLayout)
                                   .setVertexShader(m_vs[j])
                                   .setPixelShader(m_ps[j])
                                   .setVertexInputState(vertexInputState)
                                   .setPrimitiveTopology(PrimitiveTopology::eTriangleList)
                                   .setColorAttachmentFormats({ Format::eRgba8Unorm, Format::eRgba8Unorm });
        m_gfxPipeline[j] = m_device->createGraphicsPipeline(gfxPipelineDesc);
    }

    QHWI_LOGI("<< create pipelines");
#pragma endregion

    beginTransfer();
    m_copyCommandList->addTextureBarrier({ .texture = m_texImage.get(), .src = TextureState_NoAccess, .dst = TextureState_Transfer_Dst });
    m_texImage.uploadFromStagingBuffer(m_copyCommandList);
    m_copyCommandList->addTextureBarrier({ .texture = m_texImage.get(), .src = TextureState_Transfer_Dst, .dst = TextureState_PixelShader_Sample });
    endTransfer();

    return ResultCode::eSuccess;
}

void QCOMTileShadingExample::beginRendering(qhwi::CommandList* cmdList, const ClearValue& clearValue)
{
    static constexpr uint32_t NumColorAttachments = 2;
    std::array<ColorAttachmentDescription, NumColorAttachments> colorAttachments;
    for (uint32_t j = 0; j < NumColorAttachments; j++)
    {
        colorAttachments[j] = ColorAttachmentDescription()
                                  .setClearValue(clearValue)
                                  .setLoadOp(qhwi::AttachmentLoadOp::eClear)
                                  .setStoreOp(qhwi::AttachmentStoreOp::eStore)
                                  .setTexture(m_offscreenTexture.texture.Get(), Subresource().setArraySlice(j));
    }
    qhwi::TileShadingInfoQCOM tileShadingInfo;
    qhwi::TileShadingInfoQCOM* pTileShadingInfo = nullptr;
    if (m_config.useExtension)
    {
        tileShadingInfo = qhwi::TileShadingInfoQCOM().setEnableTileShading(true).setTileApronSize(s_TileApronSize);
        pTileShadingInfo = &tileShadingInfo;
    }
    cmdList->beginRenderPass(NumColorAttachments, colorAttachments.data(), nullptr, nullptr, nullptr, nullptr, pTileShadingInfo);
    uint32_t w = s_OffscreenTextureWidth, h = s_OffscreenTextureHeight;
    Viewport vp = { 0, 0, float(w), float(h), 0.0f, 1.0f };
    cmdList->setViewport(vp);
    ScissorRect scissor = { 0, 0, w, h };
    cmdList->setScissor(scissor);
}

void QCOMTileShadingExample::endRendering(qhwi::CommandList* cmdList)
{
    cmdList->endRenderPass();
}

void QCOMTileShadingExample::recordCommandList(CommandList* commandList)
{
    ClearValue clearColor{};
    clearColor.rgba = { 0.0f, 0.0f, 0.5f, 1.0f };

    m_clearColor += 1.0f / 100.0f;

    if (m_clearColor > 2.0f)
    {
        m_clearColor = 0.0f;
    }

    // Alternate clear colors
    if (m_clearColor <= 1.0f)
    {
        clearColor.rgba[1] = (m_clearColor / 2.0f) + 0.5f;
        clearColor.rgba[2] = (m_clearColor / 2.0f) + 0.5f;
    }
    else
    {
        clearColor.rgba[1] = ((2.0f - m_clearColor) / 2.0f) + 0.5f;
        clearColor.rgba[2] = ((2.0f - m_clearColor) / 2.0f) + 0.5f;
    }

    qhwi::VkCommandList* vkCommandList = qhwi::vk(commandList);
    const bool useTileExt = m_config.useExtension;

    commandList->begin();

    commandList->addTextureBarrier({ .texture = m_offscreenTexture.texture, .src = TextureState_NoAccess, .dst = TextureState_ColorAttachment });
    beginRendering(commandList, clearColor);

    uint32_t posOffset = 0;

    commandList->bindVertexBuffer(m_pos.get(), 0, posOffset, sizeof(float4));
    commandList->bindVertexBuffer(m_texCoords.get(), 1, 0, sizeof(float4));
    commandList->bindGraphicsPipeline(m_gfxPipeline[0]);
    commandList->bindGraphicsDescriptorSet(m_descSet);
    commandList->draw(3);

    if (useTileExt)
    {
        vkCommandList->beginPerTileExecutionQCOM();
    }

    posOffset += 3 * sizeof(float4);
    commandList->bindVertexBuffer(m_pos.get(), 0, posOffset, sizeof(float4));
    commandList->draw(3);

    posOffset += 3 * sizeof(float4);
    commandList->bindVertexBuffer(m_pos.get(), 0, posOffset, sizeof(float4));
    commandList->draw(3);

    if (useTileExt)
    {
        commandList->bindComputePipeline(m_cs1Pipeline);
        commandList->bindComputeDescriptorSet(m_csDescSet.Get(), m_csPipelineLayout.Get());
        commandList->addTextureBarrier({ .texture = m_offscreenTexture.texture, .src = TextureState_ColorAttachment, .dst = TextureState_ComputeShader_TileTransientReadWriteQcom },
                                       DependencyFlag_ByRegion);
        vkCommandList->dispatchTileQCOM();
        vkCommandList->endPerTileExecutionQCOM();
    }

    posOffset += 3 * sizeof(float4);
    commandList->bindVertexBuffer(m_pos.get(), 0, posOffset, sizeof(float4));
    commandList->bindVertexBuffer(m_texCoords.get(), 1, 0, sizeof(float4));
    commandList->bindGraphicsPipeline(m_gfxPipeline[1]);
    commandList->bindGraphicsDescriptorSet(m_descSet);
    if (useTileExt)
    {
        commandList->addTextureBarrier({ .texture = m_offscreenTexture.texture, .src = TextureState_ComputeShader_TileTransientReadWriteQcom, .dst = TextureState_ColorAttachment },
                                       DependencyFlag_ByRegion);
    }
    commandList->draw(3);

    if (useTileExt)
    {
        vkCommandList->beginPerTileExecutionQCOM();
        commandList->bindComputePipeline(m_cs2Pipeline.Get());
        commandList->dispatch(1, 1, 1, 1, 1, 1);
        vkCommandList->endPerTileExecutionQCOM();
    }

    endRendering(commandList);
    commandList->beginTransferPass();
    // blit the SECOND array slice from the offscreen texture to the screen
    auto s0 = Subresource({ .arraySlice = 1 });
    auto t0 = CopyTextureRegion2({ .srcSubresource = &s0 });
    commandList->addTextureBarrier({ .texture = m_offscreenTexture.texture, .src = TextureState_ColorAttachment, .dst = TextureState_Transfer_Src });
    commandList->addTextureBarrier({ .texture = m_swapchain->getCurrentImage(), .src = TextureState_NoAccess, .dst = TextureState_Transfer_Dst });
    vkCommandList->copyTexture(m_swapchain->getCurrentImage(), m_offscreenTexture.texture, t0);
    commandList->endTransferPass();
    commandList->addTextureBarrier({ .texture = m_swapchain->getCurrentImage(), .src = TextureState_Transfer_Dst, .dst = TextureState_Present });
    commandList->end();
}

enum class AppState
{
    eStart,
    eInit,
    eRun
};

static void onAppCmd(android_app* app, int32_t cmd)
{
    AppState* appState = (AppState*)app->userData;
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
    {
        ext::SetANativeWindow(app->window);
        *appState = AppState::eInit;
        break;
    }
    }
}

void android_main(android_app* state)
{
    FileUtil::setAndroidAssetManager(state->activity->assetManager);

    int argc;
    char** argv;
    GetCommandLineArgsOp op(state, "qcom_tile_shading", argc, argv);
    op.run();

    AppState appState;
    state->onAppCmd = onAppCmd;
    state->userData = &appState;
    QCOMTileShadingExample app;
    while (true)
    {
        int ident = 0;
        int events = 0;
        android_poll_source* source = nullptr;
        while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) >= 0)
        {
            if (source != nullptr)
            {
                source->process(state, source);
            }
            if (state->destroyRequested != 0)
            {
                return;
            }
        }
        if (appState == AppState::eInit)
        {
            app.init(argc, (char**)argv);
            appState = AppState::eRun;
        }
        else if (appState == AppState::eRun)
        {
            app.runIteration();
            if (app.isDone())
            {
                ANativeActivity_finish(state->activity);
            }
        }
    }
}
