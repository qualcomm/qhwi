//
//
//  qcom_tile_shading.h
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
class QCOMTileShadingExample : public Example
{
public:
    struct Config : public Example::Config
    {
    };

    ~QCOMTileShadingExample() override;
    ResultCode init(int argc, const char* const* argv);
    ResultCode init(const Config& config);

protected:
    void recordCommandList(CommandList* commandList) override;
    void beginRendering(qhwi::CommandList* cmdList, const ClearValue& clearValue);
    void endRendering(qhwi::CommandList* cmdList);

    std::span<const char* const> pilMetrics() const override
    {
        static constexpr const char* k_extras[] = { "FRAGMENTS_SHADED_PER_SECOND",
                                                    "PERCENT_TIME_SHADING_FRAGMENTS",
                                                    "PERCENT_LRZ_BUSY",
                                                    "LRZ_PIXELS_KILLED",
                                                    "PERCENT_SHADER_ALU_CAPACITY_UTILIZED" };
        return k_extras;
    }

    Config m_config;
    cxxopts::ParseResult m_params;

private:
    void initBuffer(Buffer1& b, const VkBufferDesc& desc, void* data);

    Buffer1 m_pos, m_texCoords;
    Texture1 m_texImage;
    static constexpr uint32_t s_OffscreenTextureWidth = 360;
    static constexpr uint32_t s_OffscreenTextureHeight = 240;
    Texture1 m_offscreenTexture;
    qre::RefCountPtr<Sampler> m_sampler;
    qre::RefCountPtr<qhwi::Fence> m_fence;
    Buffer1 m_computeBuffer, m_transientBuffer;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<qhwi::DescriptorSet> m_descSet;
    qre::RefCountPtr<qhwi::PipelineLayout> m_pipelineLayout;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_csDescSetLayout;
    qre::RefCountPtr<qhwi::PipelineLayout> m_csPipelineLayout, m_gfxPipelineLayout;
    qre::RefCountPtr<qhwi::DescriptorSet> m_csDescSet;
    static constexpr uint32_t s_NumGfxPipelines = 2;
    qre::RefCountPtr<qhwi::Shader> m_vs[s_NumGfxPipelines], m_ps[s_NumGfxPipelines], m_cs1, m_cs2;
    qre::RefCountPtr<qhwi::ComputePipeline> m_cs1Pipeline, m_cs2Pipeline;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_gfxPipeline[s_NumGfxPipelines];
    static constexpr uint32_t s_NumTileProperties = 1;
    qre::StaticVector<VkTilePropertiesQCOM, s_NumTileProperties> m_tileProperties;

    static constexpr uint32_t s_TextureWidth = 16;
    static constexpr uint32_t s_TextureHeight = 16;
    static constexpr uint32_t s_TileTransientBufferSize = sizeof(float4);
    static constexpr qre::math::Vec2<uint32_t> s_TileApronSize = { 2, 2 };
    float m_clearColor = 0.0f;
};
} // namespace qhwi