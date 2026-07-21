//
//
//  qhwi_example.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <chrono>
#include <string>
#include <unordered_map>
#include <cxxopts.hpp>
#include "qhwi/qhwi.h"
#include <qhwi_performance.h>
using namespace qhwi;

namespace qhwi
{

struct FrameStats
{
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    float gpuTimeMs = 0.0f; // TODO: wire up per-example via TimerQuery
    size_t dedicatedVideoMemoryBytes = 0;
    size_t sharedSystemMemoryBytes = 0;
    std::unordered_map<std::string, double> realtimeMetrics;
};

class Example
{
public:
    enum class DepthStencilType
    {
        eNone,
        eDepthOnly,
        eDepthStencil
    };

    struct Config
    {
        uint32_t numFrames = 0;
        bool useExtension = true;
        bool debug = true;
        uint32_t w = 0, h = 0;
        GraphicsBackend backend;
        bool autoBarrier = true;
        bool apiDump = false;
        bool enableVkQcomTileMemoryHeap = false;
        std::optional<std::span<qhwi::Format>> swapchainFormatCandidates = std::nullopt;
        TextureUsageMask swapchainImageUsage = TextureUsage::TextureUsage_ColorAttachment;
        bool vsync = true;
        bool enablePilMetrics = false;
        bool enableCustomResolve = false;
    };

    void runIteration();

    bool isDone() const { return m_numFrames > 0 && m_lifetimeFrameCount >= m_numFrames; }

    virtual ~Example();

    ext::Window* window() const { return m_window.get(); }

protected:
    ResultCode init(const char* name, Config* config);
    static void initCmdLineOpts(cxxopts::Options& opts);
    static void parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c);

    virtual void recordCommandList(CommandList* commandList);

    virtual void onStatsUpdated(const FrameStats& stats);

    virtual std::span<const char* const> pilMetrics() const { return {}; }

    void beginTransfer();
    void endTransfer() const;

    void beginRenderPass(qhwi::CommandList* cmdList) const;
    void endRenderPass(qhwi::CommandList* cmdList) const;

    int nextCommandListIndex() { return (m_currentCommandListIndex + 1) % m_numCommandLists; }

    qre::RefCountPtr<qhwi::Context> m_ctx;
    std::unique_ptr<qhwi::ext::Window> m_window;
    qre::RefCountPtr<qhwi::Device> m_device;
    qre::RefCountPtr<qhwi::CommandQueue> m_commandQueue;
    std::unique_ptr<qhwi::ext::Swapchain> m_swapchain;

    static const int RequestedNumSwapchainImages = 2;
    static const int MaxColorAttachments = MaxSwapchainImages;
    uint32_t m_numColorAttachments = 0;
    uint32_t m_numCommandLists = 0;
    qhwi::Format m_colorAttachmentFormat = qhwi::Format::eUnknown;
    uint32_t m_colorAttachmentWidth = 0;
    uint32_t m_colorAttachmentHeight = 0;

    qre::RefCountPtr<qhwi::CommandList> m_commandLists[MaxColorAttachments];
    qhwi::CommandList* m_currentCommandList = nullptr;
    // TODO: remove copyCommandList
    qre::RefCountPtr<qhwi::CommandList> m_copyCommandList;
    qre::RefCountPtr<qhwi::Fence> m_frameFences[MaxColorAttachments];
    int m_currentCommandListIndex = -1;

    int m_statsFrameCount = 0;
    float m_statsAccumMs = 0.0f;
    std::chrono::steady_clock::time_point m_statsLastFrameTime;
    bool m_statsInitialized = false;

    int m_lifetimeFrameCount = 0;
    float m_lifetimeTotalMs = 0.0f;
    int m_numFrames = 0;

    qhwi_metrics::RealtimeMetrics m_pil;
    bool m_pilActive = false;
    std::unordered_map<uint32_t, std::string> m_pilMetricNames;
    std::unordered_map<std::string, std::pair<double, int>> m_lifetimePilAccum;
};
} // namespace qhwi
