//
//
//  test_multiple_render_targets.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#include "test_util.h"
#include "unit_test.h"

using namespace qhwi;
using namespace test_util;

class TestMultipleRenderTargets;

class FirstRenderpassTask : test_util::Task
{
public:
    void init(Device* device, float4 color, float2* posData, uint32_t numVerts, PrimitiveTopology primitiveTopology, Format colorAttachmentFormat, Format depthStencilFormat);
    void uploadStagingBuffers(CommandList* cmdList);
    void draw(CommandList* cmdList);
    qhwi::Format getColorAttachmetFormat();

private:
    void initDescriptorSetLayouts();
    void initDescriptorSets();
    void initPipeline(PrimitiveTopology primitiveTopology, Format colorAttachmentFormat, Format depthStencilFormat);

    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayout;
    qre::RefCountPtr<DescriptorSet> m_descriptorSet;
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPos;
    Buffer1 m_constantBuffer;
    uint32_t m_numVerts = 0;
    PrimitiveTopology m_primitiveTopology;
    Format m_colorAttachmentFormat;
    float4 m_color;

    struct CbData
    {
        float4 color;
    };
};

class SecondRenderpassTask : public test_util::Task
{
public:
    void init(Device* device,
              int numTextures,
              test_util::Texture1* intermediateTextures,
              float4 color,
              float2* posData,
              uint32_t numVerts,
              test_util::float2* uvData,
              qhwi::SamplerDesc samplerDesc,
              PrimitiveTopology primitiveTopology,
              Format colorAttachmentFormat,
              Format depthStencilFormat);
    void uploadStagingBuffers(CommandList* cmdList);
    void draw(CommandList* cmdList);

private:
    void initDescriptorSetsAndLayouts();
    void initPipeline(PrimitiveTopology primitiveTopology, Format colorAttachmentFormat, Format depthStencilFormat);

    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayout;
    qre::RefCountPtr<DescriptorSet> m_descriptorSet;
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_bPos;
    Buffer1 m_bTexCords;
    int m_numTextures;
    test_util::Texture1* m_inputTextures = nullptr;
    qre::RefCountPtr<qhwi::Sampler> m_sampler;
    uint32_t m_numVerts = 0;
    PrimitiveTopology m_primitiveTopology;
};

class TestMultipleRenderTargets : public UnitTest
{
public:
    TestMultipleRenderTargets(qhwi::GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_multiple_render_targets")
    {
        m_textureWidth = config.w;
        m_textureHeight = config.h;
    }

    test_util::Texture1& getRenderpassTexture(int index);

    qhwi::ResultCode init() override;
    void recordCommandList(qhwi::CommandList* commandList) override;
    void initTextures();

    uint32_t m_textureWidth = 0, m_textureHeight = 0;
    static const int NTextureAttachments = 3;
    test_util::Texture1 m_firstRenderPassTexture[NTextureAttachments];
    static const uint32_t NumRenderpasses = 2;
    FirstRenderpassTask m_firstRenderpassTask;
    SecondRenderpassTask m_secondRenderpassTask;
};
