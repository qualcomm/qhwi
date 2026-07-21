//
//
//  test_typed_buffer.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"

using namespace qhwi;
using namespace test_util;

class TestTypedBuffer : public UnitTest
{
public:
    TestTypedBuffer(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, "test_typed_buffer")
    {
        m_config.outputType = OutputType::eBuffer;
        m_config.bufferSize = NumElements * sizeof(uvec4);
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        uint4 data[NumElements];
        for (uint32_t j = 0; j < NumElements; j++)
        {
            data[j] = { j + 1, j + 2, j + 4, j + 8 };
        }
        m_typedBuffer.initAsTypedBuffer(m_device.Get(), "uav", Format::eRgba32Uint, (void*)&data, sizeof(data));
        beginTransfer();
        m_typedBuffer.uploadFromStagingBuffer(m_copyCommandList.Get());
        endTransfer();

        initPipelines();
        return ResultCode::eSuccess;
    }

    void createDescriptorSetAndLayout()
    {
        m_device->createDescriptorSetAndLayout(
            DescriptorSetDesc().setName("typedBuffer_descSetLayout").addTypedBuffer(0, m_typedBuffer.get()).addRWStructuredBuffer(0, m_outputBuffer.get(), sizeof(uint4)),
            ShaderStage_Compute,
            0,
            m_descSetLayout,
            m_descSet);
    }

    void initPipelines()
    {
        createDescriptorSetAndLayout();

        Shader1 cs;
        initComputeShader(m_device.Get(), "testTypedBuffer", cs);

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descSetLayout));

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
        commandList->bindComputeDescriptorSet(m_descSet.Get());
        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadgroup);
        commandList->endComputePass();

        commandList->end();
    }

    qhwi::ResultCode compareOutputBuffer() override
    {
        ScopedBufferMap bufferMap;
        uint4* data = (uint4*)mapOutputBuffer(bufferMap);

        QHWI_ASSERT_MSG(data, "Unable to map buffer");

        for (uint32_t j = 0; j < NumElements; j++)
        {
            uint4 ref = { j + 1, j + 2, j + 4, j + 8 };
            if (memcmp(&data[j], &ref, sizeof(ref)) != 0)
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
    static constexpr uint32_t NumElements = 16;

    qre::RefCountPtr<DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<DescriptorSet> m_descSet;

    Buffer1 m_typedBuffer;
};

UnitTest* create_test_typed_buffer(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestTypedBuffer(backend, config);
}
