//
//
//  test_dispatch_indirect.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

constexpr uint32_t DispatchIndirectStride = 12;
constexpr uint32_t OutputBufferSizeInElements = 16;
constexpr uint32_t ReferenceValue = 0xA5A5;

enum ComputeShader
{
    ComputeShaderFillArg,
    ComputeShaderFillValue,
    ComputeShaderMax
};

class TestDispatchIndirect : public UnitTest
{
public:
    TestDispatchIndirect(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_dispatch_indirect")
    {
        m_config.outputType = OutputType::eBuffer;
        m_config.bufferSize = sizeof(uint32_t) * OutputBufferSizeInElements;
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        m_indirectArgBuffer.initAsIndirectBuffer(m_device.Get(), "indirectArg", DispatchIndirectStride);

        initPipelines();
        return ResultCode::eSuccess;
    }

    void initPipelines()
    {
        // Initialize argument buffer fill cs pipeline
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWStructuredBuffer(0, m_indirectArgBuffer.buffer, DispatchIndirectStride),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[ComputeShaderFillArg],
                                               m_descriptorSets[ComputeShaderFillArg]);

        // Initialize buffer write cs pipeline
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWStructuredBuffer(0, m_outputBuffer.buffer, sizeof(uint32_t)),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[ComputeShaderFillValue],
                                               m_descriptorSets[ComputeShaderFillValue]);

        Shader1 cs[ComputeShaderMax];
        initComputeShader(m_device.Get(), "testDispatchIndirectFillArg", cs[ComputeShaderFillArg]);
        initComputeShader(m_device.Get(), "testDispatchIndirectFillValue", cs[ComputeShaderFillValue]);

        for (int i = 0; i < ComputeShaderMax; ++i)
        {
            m_computePipelineLayout[i] = m_device->create(
                PipelineLayoutDesc()
                    .addDescriptorSetLayout(m_descriptorSetLayouts[i])
                    .addPushConstantRange({ .shaderRegister = {}, .registerSpace = {}, .shaderStages = ShaderStage_Compute, .offset = {}, .size = sizeof(uint32_t) }));

            m_computePipeline[i] = m_device->createComputePipeline({ .computeShader = cs[i].shader, .layout = m_computePipelineLayout[i] });
        }
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();

        commandList->beginTransferPass();
        m_outputBuffer.clear(commandList, 0);
        commandList->endTransferPass();

        commandList->beginComputePass();
        commandList->bindComputePipeline(m_computePipeline[ComputeShaderFillArg].Get());
        commandList->bindComputeDescriptorSet(m_descriptorSets[ComputeShaderFillArg].Get());

        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadgroup);

        commandList->bindComputePipeline(m_computePipeline[ComputeShaderFillValue].Get());
        commandList->bindComputeDescriptorSet(m_descriptorSets[ComputeShaderFillValue].Get());
        commandList->pushComputeConstants(&ReferenceValue);

        commandList->dispatchIndirect(4, 1, 1, m_indirectArgBuffer.get(), 0);
        commandList->endComputePass();

        commandList->end();
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        ScopedBufferMap bufferMap;
        uint32_t* data = (uint32_t*)mapOutputBuffer(bufferMap);

        QHWI_ASSERT_MSG(data, "Unable to map buffer");

        for (uint32_t i = 0; i < OutputBufferSizeInElements; ++i)
        {
            if (data[i] != ReferenceValue)
            {
                return ResultCode::eErrorOther;
            }
        }

        return ResultCode::eSuccess;
    }

    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout[ComputeShaderMax];
    qre::RefCountPtr<ComputePipeline> m_computePipeline[ComputeShaderMax];

    static constexpr uint32_t NumThreadGroups[] = { 1, 1, 1 };
    static constexpr uint32_t NumThreadsPerThreadgroup[] = { 1, 1, 1 };
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayouts[ComputeShaderMax];
    qre::RefCountPtr<DescriptorSet> m_descriptorSets[ComputeShaderMax];
    Buffer1 m_indirectArgBuffer;
};

UnitTest* create_test_dispatch_indirect(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDispatchIndirect(backend, config);
}
