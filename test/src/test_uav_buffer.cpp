//
//
//  test_uav_pipeline.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

#define NUM_ELEMENTS 4096

class TestUAVBuffer : public UnitTest
{
public:
    TestUAVBuffer(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_uav_buffer")
    {
        m_config.outputType = OutputType::eBuffer;
        m_config.bufferSize = sizeof(int) * NUM_ELEMENTS;
    }

    ~TestUAVBuffer()
    {
        if (m_hostBuffer1)
        {
            free(m_hostBuffer1);
            m_hostBuffer1 = nullptr;
        }
        if (m_hostBuffer2)
        {
            free(m_hostBuffer2);
            m_hostBuffer2 = nullptr;
        }
        if (m_hostBufferOut)
        {
            free(m_hostBufferOut);
            m_hostBufferOut = nullptr;
        }
    }

protected:
    ResultCode init() override
    {
        UnitTest::init();
        initBuffers();
        initDescriptorSetsAndLayouts();
        initPipelines();

        beginTransfer();
        m_inputBuffer1.uploadFromStagingBuffer(m_copyCommandList.Get());
        m_inputBuffer2.uploadFromStagingBuffer(m_copyCommandList.Get());
        endTransfer();

        computeOnCPUSide(m_hostBuffer1, m_hostBuffer2, m_hostBufferOut);

        return ResultCode::eSuccess;
    }

    void initPipelines()
    {
        Shader1 cs;
        initComputeShader(m_device.Get(), "testUavBuffer", cs);

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_computePipeline =
            m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout, .name = nullptr, .pD3DInternalExportHeader = nullptr });
    }

    void initBuffers()
    {
        m_hostBuffer1 = (int*)calloc(m_config.bufferSize, 1);
        fillArrayWithRandomNumbers(m_hostBuffer1, NUM_ELEMENTS);
        m_inputBuffer1.initAsUnorderedAccessBuffer(m_device.Get(), "InputBuffer1", m_hostBuffer1, m_config.bufferSize);
        m_hostBuffer2 = (int*)calloc(m_config.bufferSize, 1);
        fillArrayWithRandomNumbers(m_hostBuffer2, NUM_ELEMENTS);
        m_inputBuffer2.initAsUnorderedAccessBuffer(m_device.Get(), "InputBuffer2", m_hostBuffer2, m_config.bufferSize);

        m_hostBufferOut = (int*)calloc(m_config.bufferSize, 1);
    }

    void initDescriptorSetsAndLayouts()
    {
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc()
                                                   .addStructuredBuffer(0, m_inputBuffer1.get(), sizeof(int))
                                                   .addStructuredBuffer(1, m_inputBuffer2.get(), sizeof(int))
                                                   .addRWStructuredBuffer(0, m_outputBuffer.get(), sizeof(int)),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);
    }

    void computeOnCPUSide(int* input1, int* input2, int* output)
    {
        for (size_t i = 0; i < NUM_ELEMENTS; i++)
        {
            output[i] = input1[i] + input2[i];
        }
    }

    void fillArrayWithRandomNumbers(int* pFloatArray, int iSize)
    {
        for (int i = 0; i < iSize; i++)
        {
            pFloatArray[i] = i + 1;
        }
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

        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadGroup);
        commandList->endComputePass();

        commandList->end();
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        ScopedBufferMap bufferMap;
        int* gold = (int*)mapOutputBuffer(bufferMap);

        for (size_t i = 0; i < NUM_ELEMENTS; i++)
        {
            if (gold[i] == m_hostBufferOut[i])
            {
                continue;
            }
            else
            {
                QHWI_ERR("Failure : Break Value = %zd\n", i);
                return ResultCode::eErrorOther;
            }
        }
        return ResultCode::eSuccess;
    }

    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout{};
    qre::RefCountPtr<ComputePipeline> m_computePipeline{};
    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts]{};
    qre::RefCountPtr<DescriptorSet> m_descriptorSets[NumDescriptorSets]{};
    Buffer1 m_inputBuffer1{};
    Buffer1 m_inputBuffer2{};
    int* m_hostBuffer1 = nullptr;
    int* m_hostBuffer2 = nullptr;
    int* m_hostBufferOut = nullptr;
    static constexpr uint32_t NumThreadGroups[] = { 256, 1, 1 };
    static constexpr uint32_t NumThreadsPerThreadGroup[] = { 16, 1, 1 };
};

UnitTest* create_test_uav_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestUAVBuffer(backend, config);
}
