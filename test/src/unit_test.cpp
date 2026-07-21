//
//
//  unit_test.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "unit_test.h"
#include <cxxopts.hpp>
using namespace qhwi;
using namespace test_util;

UnitTest::~UnitTest()
{
    if (m_config.enableGpuCapture && m_gpuCapture)
    {
        m_gpuCapture->endFrameCapture();
    }
}

void UnitTest::beginRenderPass(qhwi::CommandList* cmdList, AttachmentLoadOp colorAttachmentLoadOp)
{
    uint32_t w = m_colorAttachmentWidth, h = m_colorAttachmentHeight;
    ColorAttachmentDescription colorAttachmentDesc = { .textureData = {},
                                                       .resolveTarget = {},
                                                       .loadOp = colorAttachmentLoadOp,
                                                       .storeOp = AttachmentStoreOp::eStore,
                                                       .clearValue = { .rgba = m_config.clearColor, .depth = 0, .stencil = 0 },
                                                       .initialState = {},
                                                       .finalState = {} };
    if (m_config.onscreen)
    {
        colorAttachmentDesc.setTexture(m_swapchain->getCurrentImage());
    }
    else
    {
        colorAttachmentDesc.setTexture(m_offscreenSurface.get());
    }

    DepthStencilAttachmentDescription depthStencilAttachmentDesc = { .textureData = {},
                                                                     .resolveTarget = {},
                                                                     .depthLoadOp = AttachmentLoadOp::eClear,
                                                                     .depthStoreOp = AttachmentStoreOp::eDontCare,
                                                                     .stencilLoadOp = AttachmentLoadOp::eClear,
                                                                     .stencilStoreOp = AttachmentStoreOp::eDontCare,
                                                                     .clearValue = { .rgba = {}, .depth = 1.0f, .stencil = 128 },
                                                                     .depthResolveMode = {},
                                                                     .stencilResolveMode = {},
                                                                     .initialState = {},
                                                                     .finalState = {} };

    bool hasDepthStencilAttachment = (m_config.depthStencilType != DepthStencilType::eNone);
    if (hasDepthStencilAttachment)
    {
        depthStencilAttachmentDesc.textureData.texture = m_depthStencilImage;
        depthStencilAttachmentDesc.textureData.subresourceRange = EntireResource;
    }

    cmdList->beginRenderPass(1, &colorAttachmentDesc, hasDepthStencilAttachment ? &depthStencilAttachmentDesc : nullptr);
    cmdList->setViewport({ .w = float(w), .h = float(h) });
    cmdList->setScissor({ .w = w, .h = h });
}

void UnitTest::endRenderPass(qhwi::CommandList* cmdList)
{
    cmdList->endRenderPass();
    if (m_config.onscreen && cmdList->desc()->supportsAutomaticBarriers)
    {
        cmdList->setState(m_swapchain->getCurrentImage(), TextureState_Present);
    }
}

qhwi::ResultCode UnitTest::compareToRefImage(const ImageData& refImageData)
{
    auto offscreenSurfaceDesc = m_offscreenSurface.texture->desc();
    uint32_t w = offscreenSurfaceDesc->w, h = offscreenSurfaceDesc->h;
    bool imageSizesMatch = (w == m_config.w && h == m_config.h);
    if (!imageSizesMatch)
    {
        return ResultCode::eErrorOther;
    }
    Buffer* offscreenSurfaceBuffer = m_offscreenSurface.readbackBuffer.get();
    ScopedBufferMap scopedDataMap(offscreenSurfaceBuffer);
    uint8_t* data = (uint8_t*)scopedDataMap.map();
    uint8_t* refData = (uint8_t*)refImageData.data;
    uint32_t pitch = m_offscreenSurface.texture->pitch();
    uint32_t refPitch = FormatUtil::getRowPitch(refImageData.getFormat(), w);
    const int Filter = 1; // Check each source pixel against pixels within window of 1

    return test_util::imageCompare(data, w, h, refData, pitch, refPitch, Filter, true, qhwi::Format::eRgba8Unorm) ? ResultCode::eSuccess : ResultCode::eErrorOther;
}

void UnitTest::doIteration()
{
    Fence* frameFence = m_frameFences[nextCommandListIndex()].Get();
    frameFence->wait();
    frameFence->reset();

    if (m_config.onscreen)
    {
        m_swapchain->acquireNextImage();
    }

    m_currentCommandListIndex = nextCommandListIndex();

    auto& commandList = m_commandLists[m_currentCommandListIndex];
    if (!commandList || !commandList->isReusable())
    {
        commandList = m_device->createCommandList(m_commandQueue.Get(), { .supportsAutomaticBarriers = m_config.autoBarrier });
    }
    m_currentCommandList = commandList.Get();

    recordCommandList(m_currentCommandList);

    CommandList* commandLists[] = { m_currentCommandList };
    m_commandQueue->executeCommandLists(1, commandLists, frameFence, (m_config.onscreen) ? m_swapchain.get() : nullptr);
    if (m_config.onscreen)
    {
        m_swapchain->presentImage(m_commandQueue.Get());
    }
}

qhwi::ResultCode UnitTest::compareOutputImage()
{
    beginTransfer();
    m_offscreenSurface.downloadToReadbackBuffer(m_copyCommandList.Get());
    endTransfer();
    char refFilePath[1024];
    qre::PrintfSafe(refFilePath, "%s/%s.png", QHWI_TEST_REF_DATA_DIR, m_name);
    if (m_config.genRefs)
    {
        test_util::TextureUtil::writeToPng(m_offscreenSurface, refFilePath);
        return ResultCode::eSuccess;
    }
    else
    {
        char filePath[1024];
        qre::PrintfSafe(filePath, "%s/%s_%s.png", QHWI_DATA_DIR, m_name, toString(m_graphicsBackend));
        test_util::TextureUtil::writeToPng(m_offscreenSurface, filePath);
        ImageData refImageData;
        auto r = ext::ImageUtil::readImage(refFilePath, refImageData);
        if (r != ext::ImageUtil::ResultCode::eSuccess)
        {
            QHWI_FATAL("read image failed: %s", refFilePath);
            return ResultCode::eErrorOther;
        }
        return compareToRefImage(refImageData);
    }
}

qhwi::ResultCode UnitTest::compareOutput()
{
    if (m_config.onscreen == true)
    {
        QHWI_TODO_MSG("add support for verifying onscreen rendering");
        return ResultCode::eErrorOther;
    }
    else if (m_config.outputType == OutputType::eImage)
    {
        return compareOutputImage();
    }
    else
    {
        return compareOutputBuffer();
    }
}

void UnitTest::runIteration()
{
    if (m_numIterationsLeft > 0)
    {
        doIteration();
    }

    m_numIterationsLeft--;

    if (m_config.onscreen)
    {
        m_window->pollEvents();

        if (m_window->shouldClose())
        {
            m_numIterationsLeft = 0;
        }
    }

    if (m_numIterationsLeft == 0)
    {
        waitUntilCompleted();
    }
}

void UnitTest::waitUntilCompleted()
{
    if (!m_currentCommandList)
    {
        return;
    }
    m_commandQueue->waitForIdle();
}

void UnitTest::createSwapchain()
{
    m_device->setWindowSurface(m_window->getSurface());
    m_swapchain.reset(
        m_device->createSwapchain(m_commandQueue.Get(), qhwi::ext::SwapchainDesc().setNumImages(NumSwapchainImages).setName("qhwi_swapchain").setAutomaticBarriers(true)));
    m_numColorAttachments = m_swapchain->getNumImages();
    m_colorAttachmentFormat = m_swapchain->getFormat();
    m_colorAttachmentWidth = m_window->getWidth();
    m_colorAttachmentHeight = m_window->getHeight();
}

void UnitTest::resizeSwapchain()
{
    waitUntilCompleted();
    for (uint32_t j = 0; j < m_numCommandLists; j++)
    {
        m_commandLists[j] = nullptr;
    }
    m_swapchain->resize(m_window->getSurface());
    m_colorAttachmentWidth = m_window->getWidth();
    m_colorAttachmentHeight = m_window->getHeight();
}

ResultCode UnitTest::init()
{
    if (m_config.enableGpuCapture)
    {
        char capturePath[128];
        qre::PrintfSafe(capturePath, "%s_%s", m_name, toString(m_graphicsBackend));
        m_gpuCapture.reset(ext::createGpuCapture(capturePath));
    }
    m_ctx = createContext({ .graphicsBackend = m_graphicsBackend, .debug = m_config.debug });
    if (m_config.outputType == OutputType::eBuffer)
    {
        m_config.onscreen = false;
    }
    if (m_config.onscreen)
    {
        char windowName[MaxStringSize];
        qre::PrintfSafe(windowName, "%s_%s", m_name, toString(m_graphicsBackend));
        m_window.reset(m_ctx->createWindow({ .name = windowName, .width = int(m_config.w), .height = int(m_config.h) }));
        m_window->setUserData(this);
        m_window->setWindowResizeCallback([](void* thiz, uint32_t w, uint32_t h) { ((UnitTest*)thiz)->resizeSwapchain(); });
    }
    m_device = m_ctx->createDevice({ .name = "qhwi_device", .emulateUma = m_config.emulateUma, .enableD3dEnhancedBarriers = !m_config.disableD3dEnhancedBarriers });

    m_isUma = m_device->isUnifiedMemoryArchitecture();
    if (m_config.enableGpuCapture && m_gpuCapture)
    {
        m_gpuCapture->setDevice(m_device.Get());
        m_gpuCapture->startFrameCapture();
    }
    m_commandQueue = m_device->createCommandQueue();
    if (m_config.onscreen)
    {
        createSwapchain();
        m_numCommandLists = m_numColorAttachments;
    }
    else if (m_config.outputType == OutputType::eImage)
    {
        if (m_colorAttachmentFormat == Format::eUnknown)
        {
            m_colorAttachmentFormat = Format::eRgba8Unorm;
        }

        m_offscreenSurface.init(m_device.Get(),
                                { .heapFlags = {},
                                  .w = m_config.w,
                                  .h = m_config.h,
                                  .d = 1,
                                  .mipLevels = 1,
                                  .arrayLayers = 1,
                                  .numPlanes = 1,
                                  .numSamples = SampleCount::e1,
                                  .format = m_colorAttachmentFormat,
                                  .rSwizzle = {},
                                  .gSwizzle = ComponentSwizzle::eG,
                                  .bSwizzle = ComponentSwizzle::eB,
                                  .aSwizzle = ComponentSwizzle::eA,
                                  .usage = TextureUsage_ColorAttachment,
                                  .type = TextureType::e2D,
                                  .name = "offscreenSurface",
                                  .automaticBarriers = true },
                                nullptr,
                                true);
        m_numColorAttachments = 1;
        m_numCommandLists = 1;
        m_colorAttachmentWidth = m_config.w;
        m_colorAttachmentHeight = m_config.h;
    }
    else if (m_config.outputType == OutputType::eBuffer)
    {
        m_numCommandLists = 1;
        m_outputBuffer.initAsUnorderedAccessBuffer(m_device.Get(), "offscreenBuffer", nullptr, m_config.bufferSize);
    }
    if (m_config.outputType == OutputType::eImage && m_config.depthStencilType != DepthStencilType::eNone)
    {
        QHWI_TODO_MSG("Support other depth formats.");
        qhwi::Format format = (m_config.depthStencilType == DepthStencilType::eDepthOnly) ? qhwi::Format::eD32Float : qhwi::Format::eD32FloatS8Uint;

        if (m_device->supportsDepthStencilFormat(format))
        {
            m_depthStencilAttachmentFormat = format;

            m_depthStencilImage = m_device->createTexture({ .heapFlags = {},
                                                            .w = m_colorAttachmentWidth,
                                                            .h = m_colorAttachmentHeight,
                                                            .d = 1,
                                                            .mipLevels = 1,
                                                            .arrayLayers = 1,
                                                            .numPlanes = 1,
                                                            .numSamples = SampleCount::e1,
                                                            .format = m_depthStencilAttachmentFormat,
                                                            .rSwizzle = {},
                                                            .gSwizzle = ComponentSwizzle::eG,
                                                            .bSwizzle = ComponentSwizzle::eB,
                                                            .aSwizzle = ComponentSwizzle::eA,
                                                            .usage = TextureUsage_DepthStencilAttachment,
                                                            .type = TextureType::e2D,
                                                            .name = "depthStencil",
                                                            .automaticBarriers = true });
        }
        else
        {
            QHWI_FATAL("Depth image format not supported");
        }
    }
    for (uint32_t j = 0; j < m_numCommandLists; j++)
    {
        char name[16];
        qre::PrintfSafe(name, "commandList[%d]", j);
        m_commandLists[j] = m_device->createCommandList(m_commandQueue.Get(), { .name = name, .supportsAutomaticBarriers = m_config.autoBarrier });
    }

    for (uint32_t j = 0; j < m_numCommandLists; j++)
    {
        m_frameFences[j] = m_device->createFence(FenceState_Signaled);
    }

    m_numIterationsLeft = m_config.onscreen ? INT_MAX : 1;
    if (m_config.maxFrames)
    {
        m_numIterationsLeft = std::min(int(m_config.maxFrames), m_numIterationsLeft);
    }

    m_isInitialized = true;

    return ResultCode::eSuccess;
}

void UnitTest::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    beginRenderPass(commandList);
    endRenderPass(commandList);
    commandList->end();
}
