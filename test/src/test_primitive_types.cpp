//
//
//  test_primitive_types.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

#define VERTEX_COUNT 4
#define SIZE         0.4f

class TestPrimitiveType : public UnitTest
{
public:
    TestPrimitiveType(GraphicsBackend graphicsBackend, Config config, const char* name, PrimitiveTopology primitiveType)
        : UnitTest(graphicsBackend, config, name)
    {
        m_primitiveType = primitiveType;
        m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_numVerts = VERTEX_COUNT * VERTEX_COUNT;
        int idx = 0;
        for (int i = -(VERTEX_COUNT / 2); i < (VERTEX_COUNT / 2); i++)
        {
            for (int j = -(VERTEX_COUNT / 2); j < (VERTEX_COUNT / 2); j++)
            {
                m_meshVertices[idx].x = (j * SIZE);
                m_meshVertices[idx].y = (-i * SIZE);
                idx++;
            }
        }
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }

        switch (m_primitiveType)
        {
        case qhwi::PrimitiveTopology::ePointList:
        {
            prepareTask();
        }
        break;
        case qhwi::PrimitiveTopology::eLineList:
        {
            const int numIndices = 16;
            uint16_t indices[numIndices] = { // Horizontal
                                             0,
                                             3,
                                             4,
                                             7,
                                             8,
                                             11,
                                             12,
                                             15,
                                             // Vertical
                                             0,
                                             12,
                                             1,
                                             13,
                                             2,
                                             14,
                                             3,
                                             15
            };
            prepareTask(indices, numIndices);
        }
        break;
        case qhwi::PrimitiveTopology::eLineStrip:
        {
            const int numIndices = 8;
            uint16_t indices[numIndices] = { 3, 0, 12, 15, 7, 5, 9, 10 };
            prepareTask(indices, numIndices);
        }
        break;
        case qhwi::PrimitiveTopology::eTriangleList:
        {
            const int numIndices = 29;
            uint16_t indices[numIndices] = { 0, 1, 4, 1, 2, 5, 2, 3, 6, 4, 5, 8, 5, 6, 9, 6, 7, 10, 8, 9, 12, 9, 10, 13, 10, 11, 14 };
            prepareTask(indices, numIndices);
        }
        break;
        case qhwi::PrimitiveTopology::eTriangleStrip:
        {
            const int numIndices = 20;
            uint16_t indices[numIndices] = { 0, 1, 4, 5, 2, 3, 6, 7, 5, 6, 9, 10, 8, 9, 12, 13, 10, 11, 14, 15 };
            prepareTask(indices, numIndices);
        }
        break;
        }

        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();

        return ResultCode::eSuccess;
    }

    void prepareTask(const uint16_t* indices = nullptr, uint32_t numIndices = 0)
    {
        m_drawColorTask.init({ .device = m_device.Get(),
                               .color = m_color,
                               .posData = m_meshVertices,
                               .numVerts = m_numVerts,
                               .indicesData = const_cast<uint16_t*>(indices),
                               .numIndices = numIndices,
                               .primitiveTopology = m_primitiveType,
                               .colorAttachmentFormat = m_colorAttachmentFormat,
                               .depthStencilFormat = m_depthStencilAttachmentFormat });
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    PrimitiveTopology m_primitiveType;
    DrawColorTask m_drawColorTask;
    uint32_t m_numVerts;
    float2 m_meshVertices[VERTEX_COUNT * VERTEX_COUNT]{};
    float4 m_color;
};

UnitTest* create_test_primitive_type_points(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestPrimitiveType(backend, config, "test_primitive_type_points", PrimitiveTopology::ePointList);
}

UnitTest* create_test_primitive_type_triangle_strip(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestPrimitiveType(backend, config, "test_primitive_type_triangle_strip", PrimitiveTopology::eTriangleStrip);
}

UnitTest* create_test_primitive_type_triangle_list(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestPrimitiveType(backend, config, "test_primitive_type_triangle_list", PrimitiveTopology::eTriangleList);
}

UnitTest* create_test_primitive_type_line_strip(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestPrimitiveType(backend, config, "test_primitive_type_line_strip", PrimitiveTopology::eLineStrip);
}

UnitTest* create_test_primitive_type_line_list(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestPrimitiveType(backend, config, "test_primitive_type_line_list", PrimitiveTopology::eLineList);
}
