//
//
//  qhwi_example.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "qhwi_example.h"
#include <qre/text/textutil.h>
#include "qhwi/qhwi_util.h"
#include "qhwi/qhwi_log_util.h"

ResultCode Example::init(const char* name, Config* config)
{
    m_ctx = createContext({ .graphicsBackend = config->backend, .debug = config->debug, .apiDump = config->apiDump });
    char windowName[MaxStringSize];
    qre::PrintfSafe(windowName, "%s_%s", name, toString(config->backend));
    m_window.reset(m_ctx->createWindow({ .name = windowName, .width = int(config->w), .height = int(config->h) }));
    m_device = m_ctx->createDevice({
        .name = "qhwi_device",
        .enableVkQcomTileMemoryHeap = config->enableVkQcomTileMemoryHeap,
        .enableCustomResolve = config->enableCustomResolve,
    });

    m_device->setWindowSurface(m_window->getSurface());

    m_commandQueue = m_device->createCommandQueue();

    auto swapchainDesc = qhwi::ext::SwapchainDesc()
                             .setNumImages(RequestedNumSwapchainImages)
                             .setName("qhwi_swapchain")
                             .setAutomaticBarriers(config->autoBarrier)
                             .setImageUsage(config->swapchainImageUsage)
                             .setVsync(config->vsync);
    if (config->swapchainFormatCandidates)
    {
        swapchainDesc.setFormatCandidates(config->swapchainFormatCandidates.value());
    }

    m_swapchain.reset(m_device->createSwapchain(m_commandQueue.Get(), swapchainDesc));
    m_numColorAttachments = m_swapchain->getNumImages();
    m_numCommandLists = m_numColorAttachments;
    m_colorAttachmentFormat = m_swapchain->getFormat();
    m_colorAttachmentWidth = m_window->getWidth();
    m_colorAttachmentHeight = m_window->getHeight();

    for (int j = 0; j < m_numCommandLists; j++)
    {
        char name[16];
        qre::PrintfSafe(name, "commandList[%d]", j);
        m_commandLists[j] = m_device->createCommandList(m_commandQueue.Get(), { .name = name, .supportsAutomaticBarriers = config->autoBarrier });
    }

    for (int j = 0; j < m_numCommandLists; j++)
    {
        m_frameFences[j] = m_device->createFence(FenceState_Signaled);
    }

    QHWI_LOGI("[config] %s  backend: %s  use-ext: %s  surface: %dx%d  debug: %s",
              name,
              toString(config->backend),
              config->useExtension ? "ON" : "OFF",
              m_colorAttachmentWidth,
              m_colorAttachmentHeight,
              config->debug ? "ON" : "OFF");

    m_numFrames = static_cast<int>(config->numFrames);

#ifdef QHWI_ENABLE_PIL
    QHWI_LOGI("[pil] compiled WITH PIL support");
#else
    QHWI_LOGI("[pil] compiled WITHOUT PIL support (QHWI_ENABLE_PIL not defined)");
#endif

    if (config->enablePilMetrics)
    {
        // Phase 1 (main thread): enumerate available metrics and select by key, then shut down.
        // PIL binds its EGL state to whichever thread calls Init(), so we must shut down here
        // before the worker thread calls Init() and takes ownership.
        auto rc = m_pil.Init();
        if (rc == qhwi_metrics::ResultCode::eSuccess)
        {
            const qhwi_metrics::RealtimeMetricProperties* available = nullptr;
            size_t count = 0;
            m_pil.GetAvailableRealtimeMetrics(&available, &count);
            if (count > 0)
            {
                const auto allowed = pilMetrics();
                for (size_t i = 0; i < count; ++i)
                {
                    const char* key = available[i].key ? available[i].key : "";
                    const char* name = available[i].name ? available[i].name : key;
                    for (const char* k : allowed)
                    {
                        if (strcmp(key, k) == 0)
                        {
                            m_pilMetricNames[available[i].metricId] = std::string(name) + " [" + key + "]";
                            m_pilSelectedIds.push_back(available[i].metricId);
                            QHWI_LOGI("[pil] monitoring: %s [%s]", name, key);
                            break;
                        }
                    }
                }
            }
            else
            {
                QHWI_LOGI("[pil] no metrics available");
            }
            m_pil.Shutdown(); // Must precede worker Init()
        }
        else
        {
            QHWI_LOGI("[pil] Init failed (result=%d)", static_cast<int>(rc));
        }

        // Phase 2: spawn worker thread that owns PIL exclusively from here on.
        if (!m_pilSelectedIds.empty())
        {
            m_pilRunning = true;
            m_pilThread = std::thread(
                [this]()
                {
                    auto rc = m_pil.Init();
                    if (rc != qhwi_metrics::ResultCode::eSuccess)
                    {
                        QHWI_LOGI("[pil] worker Init failed (result=%d)", static_cast<int>(rc));
                        m_pilRunning = false;
                        return;
                    }
                    const qhwi_metrics::RealtimeMetricID* unavailable = nullptr;
                    size_t numUnavailable = 0;
                    rc = m_pil.StartRealtimeMonitor(m_pilSelectedIds.data(), m_pilSelectedIds.size(), &unavailable, &numUnavailable);
                    if (rc != qhwi_metrics::ResultCode::eSuccess)
                    {
                        QHWI_LOGI("[pil] StartRealtimeMonitor failed (result=%d)", static_cast<int>(rc));
                        m_pil.Shutdown();
                        m_pilRunning = false;
                        return;
                    }
                    while (m_pilRunning)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        if (!m_pilRunning)
                        {
                            break;
                        }
                        const qhwi_metrics::RealtimeData* values = nullptr;
                        size_t count = 0;
                        m_pil.GetRealtimeValues(&values, &count);
                        std::lock_guard lock(m_pilValuesMutex);
                        for (size_t i = 0; i < count; ++i)
                        {
                            m_pilValuesCache[values[i].metricId] = values[i].value;
                        }
                    }
                    m_pil.StopRealtimeMonitor();
                    m_pil.Shutdown();
                });
        }
    }

    m_statsLastFrameTime = std::chrono::steady_clock::now();
    return ResultCode::eSuccess;
}

void Example::initCmdLineOpts(cxxopts::Options& opts)
{
#ifdef QHWI_BACKEND_D3D12
    const std::string defaultGraphicsBackend = "d3d12";
#elif QHWI_BACKEND_VULKAN
    const std::string defaultGraphicsBackend = "vk";
#endif
    opts.add_options()
#ifndef NDEBUG
        ("debug", "Enable debug layer", cxxopts::value<bool>()->default_value("true"))
#else
        ("debug", "Enable debug layer", cxxopts::value<bool>()->default_value("false"))
#endif
            ("width", "output surface width", cxxopts::value<uint32_t>()->default_value("0"))("height", "output surface height", cxxopts::value<uint32_t>()->default_value("0"))(
                "backend",
                "Graphics backend",
                cxxopts::value<std::string>()->default_value(defaultGraphicsBackend))("apiDump", "Enable api dump", cxxopts::value<bool>()->default_value("false"))(
                "use-ext",
                "Enable the extension code path",
                cxxopts::value<bool>()->default_value("true"))("num-frames",
                                                               "Number of frames to render before exiting (0 = unlimited)",
                                                               cxxopts::value<uint32_t>()->default_value("0"))("pil-metrics",
                                                                                                               "Enable Snapdragon Profiler Interface Library realtime metrics",
                                                                                                               cxxopts::value<bool>()->default_value("false"));
}

void Example::parse(const int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c)
{
    p = opts.parse(argc, argv);
    c.debug = p["debug"].as<bool>();
    c.w = p["width"].as<uint32_t>();
    c.h = p["height"].as<uint32_t>();
    fromString(p["backend"].as<std::string>().c_str(), c.backend);
    c.apiDump = p["apiDump"].as<bool>();
    c.useExtension = p["use-ext"].as<bool>();
    c.numFrames = p["num-frames"].as<uint32_t>();
    c.enablePilMetrics = p["pil-metrics"].as<bool>();
}

void Example::beginTransfer()
{
    if (!m_copyCommandList || !m_copyCommandList->isReusable())
    {
        m_copyCommandList = m_device->createCommandList(m_commandQueue.Get(), { .name = "copyCommandList", .supportsAutomaticBarriers = true });
    }
    m_copyCommandList->begin();
    m_copyCommandList->beginTransferPass();
}

void Example::endTransfer() const
{
    m_copyCommandList->endTransferPass();
    m_copyCommandList->end();
    qhwi::CommandList* cmdLists[] = { m_copyCommandList.Get() };
    m_commandQueue->executeCommandLists(1, cmdLists, nullptr);
    m_commandQueue->waitForIdle();
}

void Example::beginRenderPass(qhwi::CommandList* cmdList) const
{
    const uint32_t w = m_colorAttachmentWidth, h = m_colorAttachmentHeight;
    ColorAttachmentDescription colorAttachmentDesc = { .loadOp = AttachmentLoadOp::eClear, .storeOp = AttachmentStoreOp::eStore };
    colorAttachmentDesc.setTexture(m_swapchain->getCurrentImage());

    cmdList->beginRenderPass(1, &colorAttachmentDesc, nullptr);
    cmdList->setViewport({ .w = static_cast<float>(w), .h = static_cast<float>(h) });
    cmdList->setScissor({ .w = w, .h = h });
}

void Example::endRenderPass(qhwi::CommandList* cmdList) const
{
    cmdList->endRenderPass();
    cmdList->setState(m_swapchain->getCurrentImage(), TextureState_Present);
}

void Example::onStatsUpdated(const FrameStats& stats) {}

void Example::shutdown()
{
    if (m_commandQueue)
    {
        m_commandQueue->waitForIdle();
    }
}

Example::~Example()
{
    if (m_pilRunning)
    {
        m_pilRunning = false;
        if (m_pilThread.joinable())
        {
            m_pilThread.join();
        }
    }
    if (m_measuredFrameCount > 0 && m_lifetimeDuration > std::chrono::nanoseconds::zero())
    {
        const float totalMs = std::chrono::duration<float, std::milli>(m_lifetimeDuration).count();
        const float avgFps = float(m_measuredFrameCount) * 1000.0f / totalMs;
        const float avgFrameMs = totalMs / float(m_measuredFrameCount);
        const float durationSec = totalMs / 1000.0f;
        QHWI_LOGI("[stats] frames: %d  duration: %.1fs  avg FPS: %.1f  avg frame: %.2fms", m_measuredFrameCount, durationSec, avgFps, avgFrameMs);
        for (const auto& [name, acc] : m_lifetimePilAccum)
        {
            if (acc.second > 0)
            {
                QHWI_LOGI("[pil] %s: %.4g", name.c_str(), acc.first / acc.second);
            }
        }
    }
}

void Example::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    beginRenderPass(commandList);
    endRenderPass(commandList);
    commandList->end();
}

void Example::runIteration()
{
    const auto now = std::chrono::steady_clock::now();
    m_lifetimeFrameCount++;
    if (m_lifetimeFrameCount > kWarmupFrames)
    {
        const float frameMs = std::chrono::duration<float, std::milli>(now - m_statsLastFrameTime).count();

        m_measuredFrameCount++;
        m_statsAccumMs += frameMs;
        m_statsFrameCount++;
        m_lifetimeDuration += now - m_statsLastFrameTime;
        if (m_statsAccumMs >= 1000.0f)
        {
            FrameStats stats;
            stats.fps = static_cast<float>(m_statsFrameCount) * 1000.0f / m_statsAccumMs;
            stats.frameTimeMs = m_statsAccumMs / static_cast<float>(m_statsFrameCount);
            stats.dedicatedVideoMemoryBytes = m_device->getDedicatedVideoMemory();
            stats.sharedSystemMemoryBytes = m_device->getSharedSystemMemory();
            if (m_pilRunning)
            {
                std::lock_guard lock(m_pilValuesMutex);
                for (const auto& [id, value] : m_pilValuesCache)
                {
                    auto it = m_pilMetricNames.find(id);
                    if (it != m_pilMetricNames.end())
                    {
                        stats.realtimeMetrics[it->second] = value;
                    }
                }
                for (const auto& [name, value] : stats.realtimeMetrics)
                {
                    auto& [sum, n] = m_lifetimePilAccum[name];
                    sum += value;
                    ++n;
                }
            }
            onStatsUpdated(stats);
            m_statsFrameCount = 0;
            m_statsAccumMs = 0.0f;
        }
    }
    m_statsLastFrameTime = now;

    m_currentCommandListIndex = nextCommandListIndex();

    Fence* frameFence = m_frameFences[m_currentCommandListIndex].Get();
    frameFence->wait();
    frameFence->reset();

    m_swapchain->acquireNextImage();

    auto& commandList = m_commandLists[m_currentCommandListIndex];
    if (!commandList || !commandList->isReusable())
    {
        commandList = m_device->createCommandList(m_commandQueue.Get(), { .supportsAutomaticBarriers = true });
    }
    m_currentCommandList = commandList.Get();

    recordCommandList(m_currentCommandList);

    CommandList* commandLists[] = { m_currentCommandList };
    m_commandQueue->executeCommandLists(1, commandLists, frameFence, m_swapchain.get());
    m_swapchain->presentImage(m_commandQueue.Get());
}
