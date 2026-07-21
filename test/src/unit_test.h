//
//
//  unit_test.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#include "test_util.h"
#include <qhwi/qhwi_util.h>

class UnitTest
{
public:
    enum class OutputType
    {
        eImage,
        eBuffer
    };
    enum class DepthStencilType
    {
        eNone,
        eDepthOnly,
        eDepthStencil
    };

    struct Config
    {
        OutputType outputType = OutputType::eImage;
        qhwi::Color clearColor;
        uint32_t numFrames = 0;
        bool onscreen = true;
        bool enableGpuCapture = false;
        bool debug = true;
        uint32_t w = 0, h = 0;
        uint32_t bufferSize = 0;
        uint32_t maxFrames = 0;
        bool genRefs = false;
        bool emulateUma = false;
        bool disableD3dEnhancedBarriers = false;
        DepthStencilType depthStencilType = DepthStencilType::eNone;
        bool autoBarrier = true;
    };

    UnitTest(qhwi::GraphicsBackend graphicsBackend, Config config, const char* name)
        : m_graphicsBackend(graphicsBackend)
        , m_config(config)
        , m_name(name)
    {
    }

    virtual ~UnitTest();

    void runIteration();
    void drawFrame();
    qhwi::ResultCode compareOutput();

    virtual qhwi::ResultCode init();

    bool isInitialized() { return m_isInitialized; }

    inline bool isFinished() { return m_numIterationsLeft == 0; }

    virtual void recordCommandList(qhwi::CommandList* commandList);

    inline const char* getName() { return m_name; }

    virtual void beginRenderPass(qhwi::CommandList* cmdList, qhwi::AttachmentLoadOp colorAttachmentLoadOp = qhwi::AttachmentLoadOp::eClear);
    virtual void endRenderPass(qhwi::CommandList* cmdList);

    inline void beginTransfer()
    {
        if (!m_copyCommandList || !m_copyCommandList->isReusable())
        {
            m_copyCommandList = m_device->createCommandList(m_commandQueue.Get(), { .name = "copyCommandList", .supportsAutomaticBarriers = m_config.autoBarrier });
        }
        m_copyCommandList->begin();
        m_copyCommandList->beginTransferPass();
    }

    inline void endTransfer()
    {
        m_copyCommandList->endTransferPass();
        m_copyCommandList->end();
        qhwi::CommandList* cmdLists[] = { m_copyCommandList.Get() };
        m_commandQueue->executeCommandLists(1, cmdLists, nullptr);
        m_commandQueue->waitForIdle();
    }

    void waitUntilCompleted();

    inline qhwi::Device* getDevice() { return m_device.Get(); }

    inline qhwi::Format getColorAttachmentFormat() { return m_colorAttachmentFormat; }

    inline qhwi::Format getDepthStencilAttachmentFormat() { return m_depthStencilAttachmentFormat; }

    inline test_util::Buffer1* getOutputBuffer() { return &m_outputBuffer; }

    inline void* mapOutputBuffer(test_util::ScopedBufferMap& bufferMap, uint64_t offset = 0, uint64_t size = 0)
    {
        beginTransfer();
        m_outputBuffer.downloadToReadbackBuffer(m_copyCommandList.Get());
        endTransfer();
        bufferMap.buffer = m_outputBuffer.readbackBuffer.Get();

        return bufferMap.map(offset, size);
    }

    inline qhwi::GraphicsBackend getGraphicsBackend() const { return m_graphicsBackend; }

protected:
    void createSwapchain();
    void resizeSwapchain();
    void doIteration();
    virtual qhwi::ResultCode compareOutputImage();

    virtual qhwi::ResultCode compareOutputBuffer() { return qhwi::ResultCode::eErrorOther; };

    qhwi::ResultCode compareToRefImage(const qhwi::ImageData& refImageData);

    qhwi::GraphicsBackend m_graphicsBackend;
    Config m_config;
    const char* m_name = nullptr;
    std::unique_ptr<qhwi::ext::GpuCapture> m_gpuCapture;
    qre::RefCountPtr<qhwi::Context> m_ctx;
    std::unique_ptr<qhwi::ext::Window> m_window;
    qre::RefCountPtr<qhwi::Device> m_device;
    qre::RefCountPtr<qhwi::CommandQueue> m_commandQueue;
    std::unique_ptr<qhwi::ext::Swapchain> m_swapchain;

    test_util::Buffer1 m_outputBuffer;
    test_util::Texture1 m_offscreenSurface;
    qre::RefCountPtr<qhwi::Texture> m_depthStencilImage;
    static const int NumSwapchainImages = 2;
    static const int MaxColorAttachments = 4;
    uint32_t m_numColorAttachments = 0;
    uint32_t m_numCommandLists = 0;
    qhwi::Format m_colorAttachmentFormat = qhwi::Format::eUnknown;
    uint32_t m_colorAttachmentWidth = 0;
    uint32_t m_colorAttachmentHeight = 0;
    qhwi::Format m_depthStencilAttachmentFormat = qhwi::Format::eUnknown;
    qre::RefCountPtr<qhwi::CommandList> m_commandLists[MaxColorAttachments];
    qhwi::CommandList* m_currentCommandList = nullptr;
    qre::RefCountPtr<qhwi::CommandList> m_copyCommandList;
    qre::RefCountPtr<qhwi::Fence> m_frameFences[MaxColorAttachments];

    int nextCommandListIndex() { return (m_currentCommandListIndex + 1) % m_numCommandLists; }

    int m_currentCommandListIndex = -1;
    bool m_isUma = false;
    int m_numIterationsLeft = 0;
    bool m_isInitialized = false;
};
