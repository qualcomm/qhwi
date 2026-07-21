//
//
//  test_desc_set.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestDescSetUpdate : public UnitTest
{
public:
    TestDescSetUpdate(GraphicsBackend backend, UnitTest::Config config)
        : UnitTest(backend, config, "test_desc_set_update")
    {
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        float4 color = { 1, 0, 0, 1 };
        float2 pos[] = { { 0, 0 }, { 1, 1 }, { 1, 0 } };
        m_drawColorTask.init({ .device = m_device.Get(),
                               .color = color,
                               .posData = pos,
                               .numVerts = NumVerts,
                               .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                               .colorAttachmentFormat = m_colorAttachmentFormat,
                               .depthStencilFormat = m_depthStencilAttachmentFormat });
        float4 color2 = { 0, 1, 0, 1 };
        m_cb2.initAsConstantBuffer(m_device.Get(), "cb", &color2, sizeof(color2));
        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        m_cb2.uploadFromStagingBuffer(m_copyCommandList.Get());
        endTransfer();
        auto descSet = m_drawColorTask.descriptorSet(0);
        DescriptorBufferInfo bInfo = { .buffer = m_cb2.get(), .stride = sizeof(float4) };
        m_device->updateDescriptorSet(descSet, &bInfo, DescriptorType::eConstantBuffer, 0, 0, 1);
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

private:
    Buffer1 m_cb2;
    DrawColorTask m_drawColorTask;
    static constexpr uint32_t NumVerts = 3;
};

UnitTest* create_test_desc_set_update(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestDescSetUpdate(backend, config);
}