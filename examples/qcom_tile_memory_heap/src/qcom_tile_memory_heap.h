//
//
//  qcom_tile_memory_heap.h
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#include "qhwi_example.h"
// TODO: remove dependency on test_util
#include "test_util.h"
#include "qhwi/qhwi_vk.h"
using namespace test_util;

namespace qhwi
{
class BlitTextureTask2D : public Task
{
public:
    struct InitParams
    {
        qhwi::Device* device = nullptr;
        Texture1* src = nullptr;
        Texture1* dst = nullptr;
    };

    void init(const InitParams& initParams);
    void run(qhwi::CommandList* cmdList);

private:
    void initDescriptorSetsAndLayouts();
    void initPipeline();
    qhwi::Device* m_device = nullptr;
    Texture1* m_src = nullptr;
    Texture1* m_dst = nullptr;
    Shader1 m_cs;
    qre::RefCountPtr<qhwi::PipelineLayout> m_computePipelineLayout;
    qre::RefCountPtr<qhwi::ComputePipeline> m_computePipeline;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<qhwi::DescriptorSet> m_descSet;
    static constexpr uint32_t s_ThreadsPerThreadGroup[] = { 8, 8, 1 };
};

class QcomTileMemoryHeapExample : public Example
{
public:
    struct Config : public Example::Config
    {
        enum class ShaderType
        {
            hlsl,
            glsl
        };
        ShaderType shaderType = ShaderType::hlsl;
    };

    ~QcomTileMemoryHeapExample() override;
    ResultCode init(int argc, const char* const* argv);
    ResultCode init(const Config& config);

protected:
    void recordCommandList(CommandList* commandList) override;
    void initCmdLineOpts(cxxopts::Options& opts);
    void parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& config);

    std::span<const char* const> pilMetrics() const override
    {
        static constexpr const char* k_extras[] = { "READ_TOTAL", "WRITE_TOTAL", "PERCENT_STALLED_ON_SYSTEM_MEMORY", "PERCENT_L1_HIT" };
        return k_extras;
    }

    Config m_config;
    cxxopts::ParseResult m_params;
    BlitTextureTask2D m_blitTextureTask;
    test_util::DrawTextureTask2D m_drawTextureTask;
    Texture1 m_texture, m_stagingTexture;
    static constexpr uint32_t s_TextureWidth = 1280, s_TextureHeight = 800;
    bool m_useTileMemory = false;
};

} // namespace qhwi