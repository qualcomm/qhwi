//
//
//  test_renderpasses.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#include "test_util.h"
#include "unit_test.h"

class TestRenderPass : public UnitTest
{
public:
    TestRenderPass(qhwi::GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_renderpass")
    {
    }
};

class TestSubpasses;

class FirstSubpassTask : public test_util::Task
{
public:
    virtual ~FirstSubpassTask() {}

    void init(TestSubpasses* testSubpasses,
              qhwi::Device* device,
              test_util::float4 color,
              test_util::float2* posData = nullptr,
              uint32_t numVerts = 0,
              uint16_t* indicesData = nullptr,
              uint32_t numIndices = 0,
              qhwi::PrimitiveTopology primitiveTopology = qhwi::PrimitiveTopology::eTriangleStrip);
    void uploadStagingBuffers(qhwi::CommandList* cmdList);
    void draw(qhwi::CommandList* cmdList);
    static const uint32_t NumSubpassOutputs = 4;

protected:
    virtual void initDescriptorSetLayouts();
    virtual void initDescriptorSets();
    void initShaders(test_util::Shader1& vs, test_util::Shader1& ps);
    void initPipelineLayout();
    virtual void initPipeline();
    virtual void initPipelineDesc(const test_util::Shader1& vs, const test_util::Shader1& ps, qhwi::GraphicsPipelineDesc& gpDesc);
    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<qhwi::DescriptorSet> m_descriptorSets[NumDescriptorSets];
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline;
    test_util::Buffer1 m_bPos;
    test_util::Buffer1 m_bIndices;
    test_util::Buffer1 m_cb;
    static const uint32_t B_POS = 0, B_CB = 0;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
    qhwi::PrimitiveTopology m_primitiveTopology;
    test_util::float4 m_color;

    struct CbData
    {
        test_util::float4 color;
    };

    TestSubpasses* m_testSubpasses = nullptr;
};

class SecondSubpassTask : public test_util::Task
{
public:
    virtual ~SecondSubpassTask() {}

    void init(TestSubpasses* testSubpasses,
              qhwi::Device* device,
              test_util::Texture1* intermediateTextures,
              test_util::float2* posData = nullptr,
              uint32_t numVerts = 0,
              uint16_t* indicesData = nullptr,
              uint32_t numIndices = 0,
              test_util::float2* uvData = nullptr,
              qhwi::Sampler::Description samplerDesc = {});
    void draw(qhwi::CommandList* cmdList);
    void uploadStagingBuffers(qhwi::CommandList* cmdList);
    static const uint32_t NumSubpassInputs = 4;

protected:
    virtual void initDescriptorSetLayouts();
    virtual void initDescriptorSets();
    void initShaders(test_util::Shader1& vs, test_util::Shader1& ps);
    void initPipelineLayout();
    virtual void initPipeline();
    virtual void initPipelineDesc(const test_util::Shader1& vs, const test_util::Shader1& ps, qhwi::GraphicsPipelineDesc& gpDesc);

    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<qhwi::DescriptorSet> m_descriptorSets[NumDescriptorSets];
    qre::RefCountPtr<qhwi::PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline> m_graphicsPipeline;
    test_util::Buffer1 m_bPos;
    test_util::Buffer1 m_bIndices;
    test_util::Texture1* m_intermediateTextures = nullptr;
    qre::RefCountPtr<qhwi::Sampler> m_sampler;
    static const uint32_t B_POS = 0;
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
    TestSubpasses* m_testSubpasses = nullptr;
};

class TestSubpasses : public UnitTest
{
public:
    TestSubpasses(qhwi::GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_subpasses")
    {
        m_config.autoBarrier = false;
    }

    virtual ~TestSubpasses() {}

    static const uint32_t NumSubpasses = 2;

protected:
    qhwi::ResultCode init() override;

    virtual void initSubpassTasks()
    {
        m_firstSubpassTask.reset(new FirstSubpassTask());
        m_secondSubpassTask.reset(new SecondSubpassTask());
    }

    void initTextures();
    void recordCommandList(qhwi::CommandList* commandList) override;
    void beginRenderPass(qhwi::CommandList* cmdList, qhwi::AttachmentLoadOp loadOp = qhwi::AttachmentLoadOp::eClear) override;
    virtual void nextSubpass(qhwi::CommandList* cmdList);
    static const int NumIntermediateAttachments = 4;
    test_util::Texture1 m_texture[NumIntermediateAttachments];

    virtual void initRenderPass() {}

    virtual void initFramebuffers() {}

    uint32_t m_frame = 0;
    std::unique_ptr<FirstSubpassTask> m_firstSubpassTask;
    std::unique_ptr<SecondSubpassTask> m_secondSubpassTask;
};
