//
//
//  qcom_multiview.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <chrono>
#include <thread>

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <fstream>
#    include <qhwi/qhwi_android_window.h>
#endif

#include <fstream>
#include <sstream>

#include "qcom_multiview.h"
#include <qhwi/qhwi_util.h>
#include <qre/text/textutil.h>
using namespace test_util;

QcomMultiViewExample::~QcomMultiViewExample()
{
    shutdown();
}

ResultCode QcomMultiViewExample::init(int argc, const char* const* argv)
{
    cxxopts::Options opts("QcomMultiViewExample", "Run QCOM multiview example");
    Example::initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    Example::parse(argc, argv, opts, parseResult, m_config);

    Example::init("QcomMultiViewExample", &m_config);

    Shader1 vs, ps;
    m_useQcomMultiview = m_config.useExtension && m_device->supportsMultiViewPerViewViewportsQCOM() && m_device->supportsMultiViewPerViewRenderAreasQCOM();
    const char* shaderName = m_useQcomMultiview ? "qcom_multiview" : "multiview";
    initVertexAndPixelShader(m_device.Get(), shaderName, vs, ps);

    m_descSetLayout = m_device->create(DescriptorSetLayoutDesc().setName("multiview_descSetLayout"));
    m_pipelineLayout = m_device->create(PipelineLayoutDesc().setName("multiview_pipelineLayout"));
    m_pipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                    .pixelShader = ps.shader,
                                                    .layout = m_pipelineLayout,
                                                    .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                                    .colorAttachmentFormats = { Format::eRgba8Unorm },
                                                    .multiViewState = { .numViews = NumViews } });
    m_multiViewTexture.init(m_device.Get(),
                            TextureDesc()
                                .setName("multiview_texture")
                                .setWidth(m_colorAttachmentWidth)
                                .setHeight(m_colorAttachmentHeight)
                                .setType(TextureType::e2DArray)
                                .setUsage(TextureUsage_ColorAttachment | TextureUsage_Sampled)
                                .setArrayLayers(NumViews)
                                .setAutomaticBarriers(true));
    for (uint32_t j = 0; j < NumViews; j++)
    {
        m_drawTextureTask[j].init(
            { .device = m_device.Get(), .texture = &m_multiViewTexture, .colorAttachmentFormat = m_colorAttachmentFormat, .subresource = { .arraySlice = uint16_t(j) } });
    }
    beginTransfer();
    for (uint32_t j = 0; j < NumViews; j++)
    {
        m_drawTextureTask[j].uploadStagingBuffers(m_copyCommandList.Get());
    }
    endTransfer();

    return ResultCode::eSuccess;
}

void QcomMultiViewExample::beginMultiViewRenderPass(qhwi::CommandList* cmdList)
{
    uint32_t w = m_colorAttachmentWidth, h = m_colorAttachmentHeight;
    ColorAttachmentDescription colorAttachmentDesc = {
        .textureData = TextureAttachmentData().setTexture(m_multiViewTexture.get()).setSubresourceRange(EntireResource),
        .loadOp = AttachmentLoadOp::eClear,
        .storeOp = AttachmentStoreOp::eStore,
    };
    // use different viewport and scissor rect for each view
    Viewport vp[] = {
        { .x = 0, .y = 0, .w = float(w), .h = float(h) },
        { .x = float(w / 4.0f), .y = 0, .w = float(w / 2.0f), .h = float(h) },
    };
    MultiViewState multiViewState = { .numViews = NumViews };
    Rect2D perViewRenderAreasQCOM[NumViews];
    bool supportsMultiViewPerViewRenderAreasQCOM = m_useQcomMultiview;
    if (supportsMultiViewPerViewRenderAreasQCOM)
    {
        // use same render area as viewport
        for (uint32_t j = 0; j < NumViews; j++)
        {
            const auto& v = vp[j];
            perViewRenderAreasQCOM[j] = { .x = uint32_t(v.x), .y = uint32_t(v.y), .w = uint32_t(v.w), .h = uint32_t(v.h) };
        }
    }
    cmdList->beginRenderPass(1, &colorAttachmentDesc, nullptr, &multiViewState, supportsMultiViewPerViewRenderAreasQCOM ? perViewRenderAreasQCOM : nullptr);
    cmdList->setViewportsAndScissors(NumViews, vp);
}

void QcomMultiViewExample::recordCommandList(CommandList* commandList)
{
    uint32_t w = m_colorAttachmentWidth, h = m_colorAttachmentHeight;
    float w2 = float(w) / 2.0f;
    float h2 = float(h) / 2.0f;
    commandList->begin();
    // render to each layer of the multiview texture
    beginMultiViewRenderPass(commandList);
    commandList->bindGraphicsPipeline(m_pipeline.Get());
    commandList->draw(VertexCount);
    endRenderPass(commandList);
    // render the multiview texture
    beginRenderPass(commandList);
    commandList->setViewportAndScissor({ .w = w2, .h = h2 });
    m_drawTextureTask[0].draw(commandList);
    commandList->setViewportAndScissor({ .x = w2, .w = w2, .h = h2 });
    m_drawTextureTask[1].draw(commandList);
    endRenderPass(commandList);
    commandList->end();
}

#ifdef QHWI_PLATFORM_ANDROID

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
    GetCommandLineArgsOp op(state, "qcom_multiview", argc, argv);
    op.run();

    AppState appState;
    state->onAppCmd = onAppCmd;
    state->userData = &appState;
    QcomMultiViewExample app;
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
#else
int main(int argc, char** argv)
{
    qre::OsSetLogConsole(true);
    QcomMultiViewExample app;
    app.init(argc, argv);
    auto window = app.window();
    while (true)
    {
        window->pollEvents();
        if (window->shouldClose() || app.isDone())
        {
            break;
        }
        app.runIteration();
    }
    return 0;
}
#endif