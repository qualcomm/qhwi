//
//
//  graphics_pipeline_library.h
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#include "qhwi_example.h"
#include "test_util.h"
#include "qhwi/qhwi_vk.h"
#include "qhwi_performance.h"

using namespace test_util;

namespace qhwi
{
class GraphicsPipelineLibraryExample : public Example
{
    enum ShaderVariation
    {
        Permutations,
        DeadVarying,
        HeavyVec4,
        LargeUbo,
        Count
    };

    struct VSInput
    {
        float4 pos;
        float4 color;
    };

    struct CBInput
    {
        float4x4 modelViewProj;
    };

public:
    struct Config : public Example::Config
    {
        ShaderVariation shaderVariation = ShaderVariation::Permutations;
        uint32_t numPipelines = 1024;
        enum class PipelineType
        {
            monolithic,
            pipelineLibrary,
            Count
        };
        PipelineType pipelineType = PipelineType::monolithic;
        bool retainLinkTimeOpt = true;
        bool linkTimeOpt = true;
        bool runTimePipelines = false;
        int numFramesToRender = 10000;
        int numFramesPerPipeline = 10;
        int maxNumShaders = 10;
        bool shareShaders = false;
        bool usePipelineCache = true;
        bool useSmallViewport = false;
    };

    ~GraphicsPipelineLibraryExample() override;
    ResultCode init(int argc, const char* const* argv);

protected:
    void recordCommandList(CommandList* commandList) override;

    void initCmdLineOpts(cxxopts::Options& opts);
    void parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& config);

    Config m_config;
    cxxopts::ParseResult m_params;

private:
    void uploadStagingBuffers(CommandList* cmdList);
    void initialVertexBuffer();
    void pipelinePrep();
    void updateView();
    void createPipelines();
    void determineShaderName(int index);
    void pipelineLibraryCreation();
    void logReport();
    void createTestConfigurations();
    qre::RefCountPtr<qhwi::GraphicsPipeline> createPipeline(int index);

    std::vector<Config> m_configurations;

    Buffer1 m_constantbufferUBO;
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayoutUBO;
    qre::RefCountPtr<DescriptorSet> m_descriptorSetUBO;

    VertexInputState m_vertexInputState;
    RasterizerState m_rasterizerState;

    std::vector<qre::RefCountPtr<qhwi::GraphicsPipeline>> m_gfxPipelines;

    std::vector<qre::RefCountPtr<qhwi::GraphicsPipeline>> m_vertexInputPipelineLibraries;
    std::vector<qre::RefCountPtr<qhwi::GraphicsPipeline>> m_preRasterizationPipelineLibraries;
    std::vector<qre::RefCountPtr<qhwi::GraphicsPipeline>> m_fragmentPipelineLibraries;
    std::vector<qre::RefCountPtr<qhwi::GraphicsPipeline>> m_fragmentOutputPipelineLibraries;
    qre::RefCountPtr<qhwi::PipelineLayout> m_pipelineLayout;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descSetLayout;

    bool m_fastLinking;
    bool m_independentInterpolationDecoration;
    uint64_t m_pipelineLibraryCreationTime;
    uint64_t m_pipelineCreationTime;
    uint64_t m_renderingTime;

    char m_gpuDeviceName[255];

    Buffer1 m_bVerts;
    uint32_t m_numVerts = 0;
    CBInput m_cb;
    static const uint32_t B_VERTS = 0;

    qhwi_performance::PerformanceLogManager m_plm;
};
} // namespace qhwi