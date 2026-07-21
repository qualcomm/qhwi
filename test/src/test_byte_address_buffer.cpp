//
//
//  test_byte_address_buffer.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

constexpr uint32_t ReferenceValue = 0xa5a5;
constexpr uint32_t OutputBufferSizeInElements = 4;
constexpr uvec4 BufferData = { ReferenceValue, ReferenceValue, ReferenceValue, ReferenceValue };

enum TestCase
{
    TestCaseByteAddressBuffer,
    TestCaseRWByteAddressBuffer,
    TestCaseCount
};

enum BufferType
{
    BufferTypeSrc,
    BufferTypeDst,
    BufferTypeCount
};

class TestByteAddressBuffer : public UnitTest
{
public:
    TestByteAddressBuffer(GraphicsBackend graphicsBackend, Config config, TestCase testCase, const char* name)
        : UnitTest(graphicsBackend, config, name)
    {
        m_config.outputType = OutputType::eBuffer;
        m_config.bufferSize = sizeof(uvec4);
        m_testCase = testCase;
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        m_uav.initAsUnorderedAccessBuffer(m_device.Get(), "uav", (void*)&BufferData, sizeof(BufferData));
        beginTransfer();
        m_uav.uploadFromStagingBuffer(m_copyCommandList.Get());
        endTransfer();

        initPipelines();
        return ResultCode::eSuccess;
    }

    void initPipelines()
    {
        if (m_testCase == TestCaseByteAddressBuffer)
        {
            m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addByteAddressBuffer(0, m_uav.get()).addRWStructuredBuffer(0, m_outputBuffer.get(), sizeof(uvec4)),
                                                   ShaderStage_Compute,
                                                   0,
                                                   m_descriptorSetLayout,
                                                   m_descriptorSet);
        }
        else
        {
            m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addStructuredBuffer(0, m_uav.buffer, sizeof(uvec4)).addRWByteAddressBuffer(0, m_outputBuffer.get()),
                                                   ShaderStage_Compute,
                                                   0,
                                                   m_descriptorSetLayout,
                                                   m_descriptorSet);
        }

        Shader1 cs;
        const char* shaderName = m_testCase == TestCaseByteAddressBuffer ? "testByteAddressBuffer" : "testRwByteAddressBuffer";
        initComputeShader(m_device.Get(), shaderName, cs);

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayout));

        m_computePipeline =
            m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout, .name = nullptr, .pD3DInternalExportHeader = nullptr });
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();

        commandList->beginTransferPass();
        m_outputBuffer.clear(commandList, 0);
        commandList->endTransferPass();

        commandList->beginComputePass();
        commandList->bindComputePipeline(m_computePipeline.Get());
        commandList->bindComputeDescriptorSet(m_descriptorSet.Get());

        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadgroup);
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

    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout;
    qre::RefCountPtr<ComputePipeline> m_computePipeline;

    static constexpr uint32_t NumThreadGroups[] = { 1, 1, 1 };
    static constexpr uint32_t NumThreadsPerThreadgroup[] = { 1, 1, 1 };
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayout;
    qre::RefCountPtr<DescriptorSet> m_descriptorSet;
    Buffer1 m_uav;
    TestCase m_testCase;
};

UnitTest* create_test_byte_address_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestByteAddressBuffer(backend, config, TestCaseByteAddressBuffer, "test_byte_address_buffer");
}

UnitTest* create_test_rw_byte_address_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestByteAddressBuffer(backend, config, TestCaseRWByteAddressBuffer, "test_rw_byte_address_buffer");
}
