//
//
//  test_compute_pipeline.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestComputePipeline : public UnitTest
{
public:
    TestComputePipeline(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_compute_pipeline")
    {
        m_config.outputType = OutputType::eBuffer;
        m_config.bufferSize = sizeof(uint32_t) * 4;
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        initDescriptorSetsAndLayouts();
        initPipelines();
        return ResultCode::eSuccess;
    }

    void initDescriptorSetsAndLayouts()
    {
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWStructuredBuffer(0, m_outputBuffer.get(), sizeof(uint32_t)),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);
    }

    void initDescriptorSets() {}

    void initPipelines()
    {
        Shader1 cs;
        initComputeShader(m_device.Get(), "testComputePipeline", cs);

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_computePipeline =
            m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout, .name = nullptr, .pD3DInternalExportHeader = nullptr });
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();

        commandList->beginTransferPass();
        m_outputBuffer.clear(commandList);
        commandList->endTransferPass();

        commandList->beginComputePass();
        commandList->bindComputePipeline(m_computePipeline.Get());
        commandList->bindComputeDescriptorSet(m_descriptorSets[0].Get());

        // Execute a global sum
        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadgroup);
        commandList->endComputePass();
        commandList->end();
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        ScopedBufferMap bufferMap;
        uint32_t* data = (uint32_t*)mapOutputBuffer(bufferMap);
        return (data[0] == 32768) ? ResultCode::eSuccess : ResultCode::eErrorOther;
    }

    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout;
    qre::RefCountPtr<ComputePipeline> m_computePipeline;

    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    static constexpr uint32_t NumThreadGroups[] = { 16, 8, 2 };
    static constexpr uint32_t NumThreadsPerThreadgroup[] = { 16, 4, 2 };
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<DescriptorSet> m_descriptorSets[NumDescriptorSets];
    Buffer1 m_uav;
};

UnitTest* create_test_compute_pipeline(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestComputePipeline(backend, config);
}
