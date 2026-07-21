//
//
//  test_draw_indirect.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

struct VertexPosColor
{
    float2 pos;
    float4 color;
};

struct InstancePosColor
{
    float2 pos;
    float4 color;
};

struct CB
{
    union
    {
        uint32_t numIndices;
        uint32_t numVertices;
    };

    uint32_t numInstances;
};

class DrawIndirectTask : Task
{
public:
    struct InitParams
    {
        Device* device;
        Buffer* indirectArgBuffer;
        Buffer* constantBuffer;
        float4 color;
        VertexPosColor* vertexPosAndColor = nullptr;
        uint32_t numVerts = 0;
        InstancePosColor* instancePosAndColor = nullptr;
        uint32_t numInstances = 0;
        uint16_t* indicesData = nullptr;
        uint32_t numIndices = 0;
        PrimitiveTopology primitiveTopology = PrimitiveTopology::eTriangleStrip;
        Format colorAttachmentFormat;
        Format depthStencilFormat = Format::eUnknown;
    };

    void init(UnitTest* parent, const InitParams& p);
    void uploadStagingBuffers(CommandList* cmdList);
    void draw(CommandList* cmdList);
    void drawIndirect(CommandList* cmdList, Buffer* buffer, uint32_t drawCount);
    void dispatch(CommandList* cmdList, uint32_t groupCountX);

private:
    void initGraphicsPipeline(Format colorAttachmentFormat, Format depthStencilFormat);
    void initComputePipeline(Buffer* indirectArgBuffer, Buffer* constantBuffer);
    qre::RefCountPtr<PipelineLayout> m_graphicsPipelineLayout;
    qre::RefCountPtr<GraphicsPipeline> m_graphicsPipeline;
    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout;
    qre::RefCountPtr<ComputePipeline> m_computePipeline;
    qre::RefCountPtr<DescriptorSet> m_descriptorSet;
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayout;
    Buffer1 m_vertexBuffer;
    Buffer1 m_instanceBuffer;
    Buffer1 m_bIndices;
    static const uint32_t B_VertexBuffer = 0, B_InstanceBuffer = 1;
    static const uint32_t NumDescriptorSets = 1, NumDescriptorSetLayouts = 1;
    static constexpr uint32_t NumThreadsPerGroup[] = { 4, 1, 1 };
    uint32_t m_numVerts = 0;
    uint32_t m_numIndices = 0;
    uint32_t m_numInstances = 1;
    PrimitiveTopology m_primitiveTopology = PrimitiveTopology::eTriangleList;
    float4 m_color;

    struct CbData
    {
        float4 color;
    };

    UnitTest* m_parent = nullptr;
};

class TestDrawIndexedIndirect : public UnitTest
{
public:
    TestDrawIndexedIndirect(GraphicsBackend graphicsBackend, Config config);

protected:
    ResultCode init() override;
    void recordCommandList(CommandList* commandList) override;
    std::unique_ptr<GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_drawIndexedArg;
    Buffer1 m_vertexBuffer;
    Buffer1 m_indexBuffer;
    Buffer1 m_constantBuffer;
    DrawIndirectTask m_drawIndirectTask;
    uint32_t m_drawCount = 1;
};

class TestDrawIndirect : public UnitTest
{
public:
    TestDrawIndirect(GraphicsBackend graphicsBackend, Config config);

protected:
    ResultCode init() override;
    void recordCommandList(CommandList* commandList) override;
    std::unique_ptr<GraphicsPipeline> m_graphicsPipeline;
    Buffer1 m_drawArg;
    Buffer1 m_vertexBuffer;
    Buffer1 m_constantBuffer;
    DrawIndirectTask m_drawIndirectTask;
    uint32_t m_drawCount = 1;
};

UnitTest* create_test_draw_indexed_indirect(GraphicsBackend backend, UnitTest::Config config);
