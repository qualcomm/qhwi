//
//
//  qcom_tile_memory_heap.cpp
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

#include "qcom_tile_memory_heap.h"
#include <qhwi/qhwi_util.h>
#include <qre/text/textutil.h>

#define SHADER_TYPE "hlsl"

#define V(func)                                                                                                                                                                    \
    {                                                                                                                                                                              \
        vk::Result result = vk::Result(func);                                                                                                                                      \
        if (result != vk::Result::eSuccess)                                                                                                                                        \
        {                                                                                                                                                                          \
            QHWI_ERR("function: %s, result: %s", #func, vk::to_string(result).c_str());                                                                                            \
        }                                                                                                                                                                          \
    }

void BlitTextureTask2D::init(const InitParams& p)
{
    m_device = p.device;
    m_src = p.src;
    m_dst = p.dst;
    initDescriptorSetsAndLayouts();
    initPipeline();
}

void BlitTextureTask2D::run(qhwi::CommandList* cmdList)
{
    const auto desc = m_src->get()->desc();
    cmdList->bindComputePipeline(m_computePipeline);
    cmdList->bindDescriptorSet(m_descSet.Get(), PipelineBindPoint::eCompute, m_computePipelineLayout.Get());
    const uint32_t numThreadGroups[] = { desc->w / s_ThreadsPerThreadGroup[0], desc->h / s_ThreadsPerThreadGroup[1], 1 };
    cmdList->dispatch(numThreadGroups, s_ThreadsPerThreadGroup);
}

void BlitTextureTask2D::initDescriptorSetsAndLayouts()
{
    m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addStorageImage(0, m_src->get()).addStorageImage(1, m_dst->get()),
                                           ShaderStage_Compute,
                                           0,
                                           m_descSetLayout,
                                           m_descSet);
}

void BlitTextureTask2D::initPipeline()
{
    initComputeShader(m_device, "blit", m_cs);
    m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descSetLayout));
    m_computePipeline = m_device->createComputePipeline(ComputePipelineDesc().setName("blit").setLayout(m_computePipelineLayout.Get()).setComputeShader(m_cs.shader));
}

QcomTileMemoryHeapExample::~QcomTileMemoryHeapExample()
{
    shutdown();
}

ResultCode QcomTileMemoryHeapExample::init(int argc, const char* const* argv)
{
    cxxopts::Options opts("QcomTileMemoryHeapExample", "Run QCOM tile_memory_heap example");
    initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    parse(argc, argv, opts, parseResult, m_config);
    m_config.autoBarrier = true;
    std::array swapchainFormatCandidates = { qhwi::Format::eRgba8Unorm };
    m_config.swapchainFormatCandidates = swapchainFormatCandidates;
    m_config.swapchainImageUsage |= TextureUsage_Storage;
    m_config.apiDump = false;

    m_useTileMemory = m_config.useExtension;
    QHWI_LOGI("useTileMemory: %d", m_useTileMemory);
    if (m_useTileMemory)
    {
        m_config.enableVkQcomTileMemoryHeap = true;
    }

    Example::init("QcomTileMemoryHeapExample", &m_config);

    m_stagingTexture.init(m_device.Get(), TextureDesc().setWidth(s_TextureWidth).setHeight(s_TextureHeight).setUsage(TextureUsage_Storage).setAutomaticBarriers(true));
    m_stagingTexture.initStagingBuffer(m_device.Get());
    test_util::TextureUtil::readPng(m_stagingTexture, QHWI_DATA_DIR "/textures/sample.png");
    m_texture.init(m_device.Get(),
                   TextureDesc()
                       .setWidth(s_TextureWidth)
                       .setHeight(s_TextureHeight)
                       .setUsage(TextureUsage_Sampled | TextureUsage_Storage | (m_useTileMemory ? TextureUsage_TileMemoryQCOM : 0))
                       .setAutomaticBarriers(true));
    m_blitTextureTask.init({
        .device = m_device.Get(),
        .src = &m_stagingTexture,
        .dst = &m_texture,
    });

    m_drawTextureTask.init({ .device = m_device.Get(), .texture = &m_texture, .colorAttachmentFormat = m_colorAttachmentFormat });
    if (!m_copyCommandList || !m_copyCommandList->isReusable())
    {
        m_copyCommandList = m_device->createCommandList(m_commandQueue.Get(), { .name = "copyCommandList", .supportsAutomaticBarriers = true });
    }
    m_copyCommandList->begin();
    m_copyCommandList->beginTransferPass();
    m_stagingTexture.uploadFromStagingBuffer(m_copyCommandList.Get());
    m_drawTextureTask.uploadStagingBuffers(m_copyCommandList.Get());
    m_copyCommandList->endTransferPass();
    m_copyCommandList->beginComputePass();
    m_blitTextureTask.run(m_copyCommandList.Get());
    m_copyCommandList->endComputePass();
    m_copyCommandList->end();
    qhwi::CommandList* cmdLists[] = { m_copyCommandList.Get() };
    m_commandQueue->executeCommandLists(1, cmdLists, nullptr);
    m_commandQueue->waitForIdle();

    return ResultCode::eSuccess;
}

void QcomTileMemoryHeapExample::initCmdLineOpts(cxxopts::Options& opts)
{
    Example::initCmdLineOpts(opts);
    opts.add_options()("shader-type", "0=hlsl, 1=glsl", cxxopts::value<uint32_t>()->default_value("0"));
}

void QcomTileMemoryHeapExample::parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c)
{
    Example::parse(argc, argv, opts, p, c);
    c.shaderType = (Config::ShaderType)p["shader-type"].as<uint32_t>();
}

void QcomTileMemoryHeapExample::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    if (m_useTileMemory)
    {
        commandList->beginComputePass();
        m_blitTextureTask.run(commandList);
        commandList->endComputePass();
    }
    beginRenderPass(commandList);
    m_drawTextureTask.draw(commandList);
    endRenderPass(commandList);
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
    GetCommandLineArgsOp op(state, "qcom_tile_memory_heap", argc, argv);
    op.run();

    AppState appState;
    state->onAppCmd = onAppCmd;
    state->userData = &appState;
    QcomTileMemoryHeapExample app;
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
