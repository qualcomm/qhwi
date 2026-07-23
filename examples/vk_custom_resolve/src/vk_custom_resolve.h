#pragma once

//
//
//  vk_custom_resolve.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#include <unordered_map>
#include <qhwi/qhwi_vk.h>

#include "qhwi_example.h"
// TODO: remove dependency on test_util
#include "test_util.h"

namespace qhwi
{
class VkCustomResolveExample : public Example
{
public:
    ResultCode init(int argc, const char* const* argv);

private:
    void initCmdLineOpts(cxxopts::Options& opts);
    void parse(int argc, const char* const* argv, cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c);
    void recordCommandList(CommandList* commandList) override;
    bool usesStandardResolvePath() const;

    Config m_config;
    bool m_supportsCustomResolve = false;
    // Gradient draw pass
    qre::RefCountPtr<PipelineLayout> m_gradientPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_gradientPipeline;
    // Textures
    test_util::Texture1 m_msaaTexture;
    test_util::Texture1 m_resolvedTexture;
    // Custom resolve pass (only when m_supportsCustomResolve)
    qre::RefCountPtr<DescriptorSetLayout> m_customResolveDescSetLayout;
    qre::RefCountPtr<DescriptorSet> m_customResolveDescSet;
    qre::RefCountPtr<PipelineLayout> m_customResolvePipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_customResolvePipeline;
    // Tonemap display pass (standard path only)
    qre::RefCountPtr<Sampler> m_tonemapSampler;
    qre::RefCountPtr<DescriptorSetLayout> m_tonemapDescSetLayout;
    qre::RefCountPtr<DescriptorSet> m_tonemapDescSet;
    qre::RefCountPtr<PipelineLayout> m_tonemapPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_tonemapPipeline;
    static constexpr uint32_t VertexCount = 3;
    static constexpr SampleCount kSampleCount = SampleCount::e4;
    bool m_supportsQcomShaderResolve = false;

    // QCOM render_pass_shader_resolve path
    test_util::Texture1 m_qcomMsaaTexture;
    vk::UniqueRenderPass m_qcomRenderPass;
    std::unordered_map<VkImage, vk::UniqueFramebuffer> m_qcomFramebufferCache;
    vk::UniquePipelineLayout m_qcomGradientPipelineLayout;
    vk::UniquePipeline m_qcomGradientPipeline;
    vk::UniqueDescriptorPool m_qcomDescriptorPool;
    vk::UniqueDescriptorSetLayout m_qcomResolveDescSetLayout;
    vk::DescriptorSet m_qcomResolveDescSet;
    vk::UniquePipelineLayout m_qcomResolvePipelineLayout;
    vk::UniquePipeline m_qcomResolvePipeline;
    void initQcomPath(qhwi::VkDevice* vkDev);
    void ensureQcomFramebuffer(vk::Device device, vk::ImageView swapchainView, vk::Image swapchainImage);
    void recordQcomPath(CommandList* commandList);
};
} // namespace qhwi
