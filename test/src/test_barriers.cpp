//
//
//  test_barriers.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <functional>
#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;
template <typename T>
using sp = qre::RefCountPtr<T>;

#define CompMul(v) v[0] * v[1] * v[2]

namespace test_barriers
{

class TestBarriers;

class TestBarriersTask : public Task
{
public:
    friend class TestBarriers;

    TestBarriersTask(TestBarriers* parent)
        : m_parent(parent)
    {
    }

    virtual ~TestBarriersTask() {}

    virtual void init(Device* device)
    {
        Task::init(device);
        initDescriptorSetsAndLayouts();
        initPipelines();
    }

    virtual void run(CommandList* cmdList) = 0;
    virtual const char* getName() = 0;

protected:
    virtual void initDescriptorSetsAndLayouts() {}

    virtual void initPipelines() {}

    virtual void uploadResources(CommandList* cmdList) {}

    TestBarriers* m_parent = nullptr;
    TestBarriersTask* m_prev = nullptr;
};

class TestBarriers : public UnitTest
{
public:
    TestBarriers(GraphicsBackend graphicsBackend, Config config)
        : UnitTest(graphicsBackend, config, m_testName)
    {
    }

    void setTasks(std::vector<std::shared_ptr<TestBarriersTask>> tasks)
    {
        m_tasks = tasks;
        for (uint32_t j = 1; j < tasks.size(); j++)
        {
            m_tasks[j]->m_prev = m_tasks[j - 1].get();
        }
        qre::PrintfSafe(m_testName, "test_barriers_%s_%s", tasks[0]->getName(), tasks[1]->getName());
    }

    std::function<void()> onInitResources;
    std::function<void(CommandList*)> onUploadResources;
    std::function<ResultCode()> onCompareOutputBuffer;

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        if (onInitResources)
        {
            onInitResources();
        }
        for (auto t : m_tasks)
        {
            t->init(m_device.Get());
        }
        beginTransfer();
        if (onUploadResources)
        {
            onUploadResources(m_copyCommandList.Get());
        }
        for (auto t : m_tasks)
        {
            t->uploadResources(m_copyCommandList.Get());
        }
        endTransfer();

        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        for (auto t : m_tasks)
        {
            t->run(commandList);
        }
        commandList->end();
    }

    qhwi::ResultCode compareOutputBuffer() override { return onCompareOutputBuffer ? onCompareOutputBuffer() : ResultCode::eUnsupported; }

    std::vector<std::shared_ptr<TestBarriersTask>> m_tasks;
    char m_testName[1024];
};

class BufferParam
{
public:
    Buffer1* get() const { return (m_v0 ? m_v0.get() : m_v1); }

    inline void set(std::shared_ptr<Buffer1> v) { m_v0 = v; }

    inline void set(Buffer1* v) { m_v1 = v; }

    Buffer1* operator->() const { return get(); }

protected:
    std::shared_ptr<Buffer1> m_v0;
    Buffer1* m_v1 = nullptr;
};

class ComputeWriteTask0 : public TestBarriersTask
{
public:
    ComputeWriteTask0(TestBarriers* parent, BufferParam buffer)
        : TestBarriersTask(parent)
    {
        m_buffer = buffer;
    }

    void run(CommandList* cmdList) override
    {
        cmdList->beginComputePass();
        cmdList->bindComputePipeline(m_computePipeline.Get());
        cmdList->bindComputeDescriptorSet(m_descriptorSets[0].Get());
        cmdList->dispatch(NumThreadgroups, NumThreadsPerThreadgroup);
        cmdList->endComputePass();
    }

    const char* getName() override { return "compute_write_0"; }

    struct BufferData
    {
        uint32_t v;
    };

    static constexpr uint32_t NumThreadgroups[] = { 1024, 1, 1 }, NumThreadsPerThreadgroup[] = { 1024, 1, 1 };

protected:
    void initDescriptorSetsAndLayouts() override
    {
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWStructuredBuffer(0, m_buffer->buffer, sizeof(ComputeWriteTask0::BufferData)),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);
    }

    void initPipelines() override
    {
        Shader1 cs;
        initComputeShader(m_device, "testBarriers", cs, "ComputeWriteTask0");

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
    }

    static const uint32_t NumDescriptorSetLayouts = 1;
    sp<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    static const uint32_t NumDescriptorSets = 1;
    sp<DescriptorSet> m_descriptorSets[NumDescriptorSets];
    BufferParam m_buffer;
    sp<PipelineLayout> m_computePipelineLayout;
    sp<ComputePipeline> m_computePipeline;
};

class ComputeReadTask0 : public TestBarriersTask
{
public:
    ComputeReadTask0(TestBarriers* parent, BufferParam bufferIn, size_t bufferInStride, BufferParam bufferOut)
        : TestBarriersTask(parent)
    {
        m_bufferIn = bufferIn;
        m_bufferOut = bufferOut;
    }

    void run(CommandList* cmdList) override
    {
        cmdList->beginComputePass();
        cmdList->bindComputePipeline(m_computePipeline.Get());
        cmdList->bindComputeDescriptorSet(m_descriptorSets[0].Get());
        cmdList->dispatch(NumThreadgroups, NumThreadsPerThreadgroup);
        cmdList->endComputePass();
    }

    const char* getName() override { return "compute_read_0"; }

    static constexpr uint32_t NumThreadgroups[] = { 1024, 1, 1 }, NumThreadsPerThreadgroup[] = { 1024, 1, 1 };

protected:
    void initDescriptorSetsAndLayouts() override
    {
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc()
                                                   .addStructuredBuffer(0, m_bufferIn->buffer, sizeof(ComputeWriteTask0::BufferData))
                                                   .addRWStructuredBuffer(0, m_bufferOut->buffer, sizeof(ComputeWriteTask0::BufferData)),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);
    }

    void initPipelines() override
    {
        Shader1 cs;
        initComputeShader(m_device, "testBarriers", cs, "ComputeReadTask0");

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
    }

    static const uint32_t NumDescriptorSetLayouts = 1;
    sp<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    static const uint32_t NumDescriptorSets = 1;
    sp<DescriptorSet> m_descriptorSets[NumDescriptorSets];
    BufferParam m_bufferIn, m_bufferOut;
    BufferState m_bufferInSrcState;
    sp<PipelineLayout> m_computePipelineLayout;
    sp<ComputePipeline> m_computePipeline;
};

class DrawReadTask0 : public TestBarriersTask
{
public:
    DrawReadTask0(TestBarriers* parent, std::shared_ptr<Buffer1> vertexBuffer, std::shared_ptr<Buffer1> indexBuffer)
        : TestBarriersTask(parent)
    {
        m_bPos = vertexBuffer;
        m_bIndex = indexBuffer;
    }

    void run(CommandList* cmdList) override
    {
        m_parent->beginRenderPass(cmdList);
        cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        cmdList->bindVertexBuffer(m_bPos->get(), B_POS, 0, 2 * sizeof(float));
        cmdList->bindIndexBuffer(m_bIndex->get());
        cmdList->drawIndexed(NumVerts);
        m_parent->endRenderPass(cmdList);
    }

    const char* getName() override { return "draw_read_0"; }

protected:
    void initPipelines() override
    {
        Shader1 vs, ps;
        initVertexShader(m_device, "testBarriers", vs, "DrawReadTask0_VS_Main");
        initPixelShader(m_device, "testBarriers", ps, "DrawReadTask0_PS_Main");

        m_graphicsPipelineLayout = m_device->createPipelineLayout({});

        m_graphicsPipeline = m_device->createGraphicsPipeline({ .vertexShader = vs.shader,
                                                                .pixelShader = ps.shader,
                                                                .domainShader = {},
                                                                .hullShader = {},
                                                                .geometryShader = {},
                                                                .amplificationShader = {},
                                                                .meshShader = {},
                                                                .blendState = {},
                                                                .rasterizerState = {},
                                                                .multisampleState = {},
                                                                .depthStencilState = {},
                                                                .vertexInputState = { .vertexInputAttributeDescriptions = { { .semanticName = "POSITION",
                                                                                                                              .semanticIndex = 0,
                                                                                                                              .binding = B_POS,
                                                                                                                              .format = Format::eRg32Float,
                                                                                                                              .offset = {},
                                                                                                                              .stride = sizeof(float2),
                                                                                                                              .inputRate = {} } } },
                                                                .viewportState = {},
                                                                .scissorState = {},
                                                                .layout = m_graphicsPipelineLayout,
                                                                .primitiveTopology = PrimitiveTopology::eLineList,
                                                                .primitiveRestartValue = {},
                                                                .colorAttachmentFormats = { m_parent->getColorAttachmentFormat() },
                                                                .depthStencilFormat = m_parent->getDepthStencilAttachmentFormat(),
                                                                .meshShaderParams = {},
                                                                .multiViewState = {},
                                                                .colorAttachmentRemapping = {},
                                                                .name = {} });
    }

    static constexpr int NumVerts = 2 * 4;
    sp<PipelineLayout> m_graphicsPipelineLayout;
    sp<GraphicsPipeline> m_graphicsPipeline;
    const uint32_t B_POS = 0;
    std::shared_ptr<Buffer1> m_bPos, m_bIndex;
};

class ComputeWriteTask1 : public TestBarriersTask
{
public:
    ComputeWriteTask1(TestBarriers* parent, std::shared_ptr<Buffer1> buffer0)
        : TestBarriersTask(parent)
    {
        m_buffer = buffer0;
    }

    void run(CommandList* cmdList) override
    {
        cmdList->beginTransferPass();
        cmdList->clear(m_buffer->get());
        cmdList->endTransferPass();

        cmdList->beginComputePass();
        cmdList->bindComputePipeline(m_computePipeline.Get());
        cmdList->bindComputeDescriptorSet(m_descriptorSets[0].Get());
        cmdList->dispatch(NumThreadgroups, NumThreadsPerThreadgroup);
        cmdList->endComputePass();
    }

    const char* getName() override { return "compute_write_1"; }

    struct BufferData
    {
        uint2 i;
    };

    static constexpr uint32_t NumThreadgroups[] = { 1, 1, 1 }, NumThreadsPerThreadgroup[] = { 4, 1, 1 };

protected:
    void initDescriptorSetsAndLayouts() override
    {
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWStructuredBuffer(0, m_buffer->buffer, sizeof(ComputeWriteTask0::BufferData)),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);
    }

    void initPipelines() override
    {
        Shader1 cs;
        initComputeShader(m_device, "testBarriers", cs, "ComputeWriteTask1");

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
    }

    std::shared_ptr<Buffer1> m_buffer;
    sp<PipelineLayout> m_computePipelineLayout;
    sp<ComputePipeline> m_computePipeline;
    static const uint32_t NumDescriptorSetLayouts = 1;
    sp<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    static const uint32_t NumDescriptorSets = 1;
    sp<DescriptorSet> m_descriptorSets[NumDescriptorSets];
};

class ComputeWriteTask2 : public TestBarriersTask
{
public:
    ComputeWriteTask2(TestBarriers* parent, std::shared_ptr<Texture1> texture)
        : TestBarriersTask(parent)
    {
        m_texture = texture;
    }

    void run(CommandList* cmdList) override
    {
        cmdList->beginComputePass();
        cmdList->bindComputePipeline(m_computePipeline.Get());
        cmdList->bindComputeDescriptorSet(m_descriptorSets[0].Get());
        cmdList->dispatch(NumThreadgroups, NumThreadsPerThreadgroup);
        cmdList->endComputePass();
    }

    const char* getName() override { return "compute_write_1"; }

    struct BufferData
    {
        uint2 i;
    };

    static constexpr uint32_t NumThreadgroups[] = { 1, 1, 1 }, NumThreadsPerThreadgroup[] = { 32, 32, 1 };

protected:
    void initDescriptorSetsAndLayouts() override
    {
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWTexture(0, m_texture->texture), ShaderStage_Compute, 0, m_descriptorSetLayouts[0], m_descriptorSets[0]);
    }

    void initPipelines() override
    {
        Shader1 cs;
        initComputeShader(m_device, "testBarriers", cs, "ComputeWriteTask2");

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
    }

    std::shared_ptr<Texture1> m_texture;
    sp<PipelineLayout> m_computePipelineLayout;
    sp<ComputePipeline> m_computePipeline;
    static const uint32_t NumDescriptorSetLayouts = 1;
    sp<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    static const uint32_t NumDescriptorSets = 1;
    sp<DescriptorSet> m_descriptorSets[NumDescriptorSets];
};

class DrawSampleTask0 : public TestBarriersTask
{
public:
    DrawSampleTask0(TestBarriers* parent, std::shared_ptr<Texture1> texture)
        : TestBarriersTask(parent)
    {
        m_texture = texture;
    }

    void init(Device* device) override
    {
        TestBarriersTask::init(device);
        m_drawTextureTask.init({ .device = m_device,
                                 .texture = m_texture.get(),
                                 .posData = {},
                                 .numVerts = {},
                                 .indicesData = {},
                                 .numIndices = {},
                                 .uvData = {},
                                 .samplerDesc = {},
                                 .colorAttachmentFormat = m_parent->getColorAttachmentFormat(),
                                 .depthStencilFormat = {},
                                 .subresource = {} });
    }

    void uploadResources(CommandList* cmdList) override { m_drawTextureTask.uploadStagingBuffers(cmdList); }

    void run(CommandList* cmdList) override
    {
        m_parent->beginRenderPass(cmdList);
        m_drawTextureTask.draw(cmdList);
        m_parent->endRenderPass(cmdList);
    }

    const char* getName() override { return "draw_sample_0"; }

protected:
    std::shared_ptr<Texture1> m_texture;
    DrawTextureTask2D m_drawTextureTask;
};

class DrawSampleTask1 : public TestBarriersTask
{
public:
    DrawSampleTask1(TestBarriers* parent, std::shared_ptr<Texture1> texture, float frameBufferWidth, float frameBufferHeight)
        : TestBarriersTask(parent)
    {
        m_texture = texture;

        m_drawTextureTask.setFrameBufferSize(frameBufferWidth, frameBufferHeight);
        m_drawTextureTask.samplePixelCenter(false);
    }

    void init(Device* device) override
    {
        TestBarriersTask::init(device);
        m_drawTextureTask.init({ .device = m_device,
                                 .texture = m_texture.get(),
                                 .posData = {},
                                 .numVerts = {},
                                 .indicesData = {},
                                 .numIndices = {},
                                 .uvData = {},
                                 .samplerDesc = {},
                                 .colorAttachmentFormat = m_parent->getColorAttachmentFormat(),
                                 .depthStencilFormat = {},
                                 .subresource = {} });
    }

    void uploadResources(CommandList* cmdList) override { m_drawTextureTask.uploadStagingBuffers(cmdList); }

    void run(CommandList* cmdList) override
    {
        m_parent->beginRenderPass(cmdList);
        m_drawTextureTask.draw(cmdList);
        m_parent->endRenderPass(cmdList);
    }

    const char* getName() override { return "draw_sample_0"; }

protected:
    std::shared_ptr<Texture1> m_texture;
    DrawTextureTask2D m_drawTextureTask;
};

class DrawRenderTask0 : public TestBarriersTask
{
public:
    DrawRenderTask0(TestBarriers* parent, std::shared_ptr<Texture1> textureIn, std::shared_ptr<Texture1> textureOut)
        : TestBarriersTask(parent)
    {
        m_textureIn = textureIn;
        m_textureOut = textureOut;
    }

    void init(Device* device) override
    {
        TestBarriersTask::init(device);
        m_drawTextureTask.init({ .device = m_device,
                                 .texture = m_textureIn.get(),
                                 .posData = {},
                                 .numVerts = {},
                                 .indicesData = {},
                                 .numIndices = {},
                                 .uvData = {},
                                 .samplerDesc = {},
                                 .colorAttachmentFormat = m_textureOut->get()->desc()->format,
                                 .depthStencilFormat = {},
                                 .subresource = {} });
    }

    void uploadResources(CommandList* cmdList) override { m_drawTextureTask.uploadStagingBuffers(cmdList); }

    void run(CommandList* cmdList) override
    {
        auto textureOutDesc = m_textureOut->get()->desc();
        uint32_t w = textureOutDesc->w, h = textureOutDesc->h;
        ColorAttachmentDescription colorAttachmentDesc = { .textureData = TextureAttachmentData{ .texture = m_textureOut->get(), .subresource = {}, .subresourceRange = {} },
                                                           .resolveTarget = {},
                                                           .loadOp = AttachmentLoadOp::eClear,
                                                           .storeOp = AttachmentStoreOp::eStore,
                                                           .clearValue = {},
                                                           .initialState = {},
                                                           .finalState = {} };

        cmdList->beginRenderPass(1, &colorAttachmentDesc, nullptr);
        cmdList->setViewport({ .w = float(w), .h = float(h) });
        cmdList->setScissor({ .w = w, .h = h });
        m_drawTextureTask.draw(cmdList);
        m_parent->endRenderPass(cmdList);
    }

    const char* getName() override { return "draw_render_0"; }

protected:
    std::shared_ptr<Texture1> m_textureIn, m_textureOut;
    DrawTextureTask2D m_drawTextureTask;
};

class ComputeReadTask1 : public TestBarriersTask
{
public:
    ComputeReadTask1(TestBarriers* parent, std::shared_ptr<Texture1> textureIn, std::shared_ptr<Texture1> textureOut)
        : TestBarriersTask(parent)
    {
        m_textureIn = textureIn;
        m_textureOut = textureOut;
    }

    void run(CommandList* cmdList) override
    {
        cmdList->beginComputePass();
        cmdList->bindComputePipeline(m_computePipeline.Get());
        cmdList->bindComputeDescriptorSet(m_descriptorSets[0].Get());
        cmdList->dispatch(NumThreadgroups, NumThreadsPerThreadgroup);
        cmdList->endComputePass();
    }

    const char* getName() override { return "compute_read_1"; }

    static constexpr uint32_t NumThreadgroups[] = { 1, 1, 1 }, NumThreadsPerThreadgroup[] = { 32, 32, 1 };

protected:
    void initDescriptorSetsAndLayouts() override
    {
        m_descriptorSetLayouts[0] = m_device->create(DescriptorSetLayoutDesc().addRWTexture(0, ShaderStage_Compute).addRWTexture(1, ShaderStage_Compute));

        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWTexture(0, m_textureIn->texture).addRWTexture(1, m_textureOut->texture),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);
    }

    void initPipelines() override
    {
        Shader1 cs;
        initComputeShader(m_device, "testBarriers", cs, "ComputeReadTask1");

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));

        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
    }

    static const uint32_t NumDescriptorSetLayouts = 1;
    sp<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    static const uint32_t NumDescriptorSets = 1;
    sp<DescriptorSet> m_descriptorSets[NumDescriptorSets];
    static const uint32_t NumTextures = 2;
    std::shared_ptr<Texture1> m_textureIn, m_textureOut;
    sp<PipelineLayout> m_computePipelineLayout;
    sp<ComputePipeline> m_computePipeline;
};

class TestBarriersBindVbUpdate : public UnitTest
{
public:
    TestBarriersBindVbUpdate(GraphicsBackend backend, UnitTest::Config config)
        : UnitTest(backend, config, "test_barriers_bind_vb_update")
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
        beginTransfer();
        m_drawColorTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask.draw(commandList);
        endRenderPass(commandList);
        float2 pos[] = { { 0, 0 }, { 1, 0 }, { 1, -1 } };
        commandList->beginTransferPass();
        commandList->updateBuffer(m_drawColorTask.bPos().get(), 0, pos, sizeof(pos));
        commandList->endTransferPass();
        beginRenderPass(commandList);
        m_drawColorTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

private:
    DrawColorTask m_drawColorTask;
    static constexpr uint32_t NumVerts = 3;
};

// A variant of drawColorTask that uses the same buffer for both verts and indices
// as an additional test case for barriers
class DrawColorTask1 : public DrawColorTask
{
public:
    void init(const InitParams& p)
    {
        Task::init(p.device);
        m_color = p.color;
        m_primitiveTopology = p.primitiveTopology;
        m_numVerts = p.numVerts;
        m_numIndices = p.numIndices;

        CbData cbData = { .color = m_color };
        float2* posData = p.posData;
        m_cb.initAsConstantBuffer(m_device, "cb", &cbData, sizeof(CbData));
        uint32_t posSize = m_numVerts * sizeof(float2);
        uint32_t indicesSize = m_numIndices * sizeof(uint16_t);
        BufferDesc bPosDesc = { .heapType = HeapType::eDefault,
                                .heapFlags = {},
                                .usage = BufferUsage_VertexBuffer | BufferUsage_IndexBuffer,
                                .size = posSize + indicesSize,
                                .format = {},
                                .name = "pos",
                                .automaticBarriers = true };
        m_bPos.init(m_device, bPosDesc);
        m_bPos.stagingBuffer =
            m_device->createBuffer({ .heapType = HeapType::eUpload, .heapFlags = {}, .usage = {}, .size = bPosDesc.size, .format = {}, .name = {}, .automaticBarriers = {} });
        uint8_t* dstData = (uint8_t*)m_bPos.stagingBuffer->map();
        memcpy(dstData, posData, posSize);
        dstData += posSize;
        memcpy(dstData, p.indicesData, indicesSize);
        m_bPos.stagingBuffer->unmap();
        initDescriptorSetsAndLayouts();
        initPipeline(p.primitiveTopology, p.colorAttachmentFormat, p.depthStencilFormat);
    }

    void uploadStagingBuffers(CommandList* cmdList) override
    {
        m_bPos.uploadFromStagingBuffer(cmdList);
        m_cb.uploadFromStagingBuffer(cmdList);
    }

    void draw(CommandList* cmdList) override
    {
        bool hasIndices = (m_numIndices != 0);
        cmdList->bindGraphicsPipeline(m_graphicsPipeline.Get());
        cmdList->bindVertexBuffer(m_bPos.get(), B_POS, 0, sizeof(float2));
        if (hasIndices)
        {
            cmdList->bindIndexBuffer(m_bPos.get(), IndexFormat::eUint16, m_numVerts * sizeof(float2));
        }
        cmdList->bindGraphicsDescriptorSet(m_descriptorSets[0].Get());
        if (hasIndices)
        {
            cmdList->drawIndexed(m_numIndices, 1);
        }
        else
        {
            cmdList->draw(m_numVerts);
        }
    }
};

class TestBarriersBindVbIbBindVb : public UnitTest
{
public:
    TestBarriersBindVbIbBindVb(GraphicsBackend backend, UnitTest::Config config, const char* name = "test_barriers_bind_vb_ib_bind_vb")
        : UnitTest(backend, config, name)
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
        float2 pos0[] = { { 0, 0 }, { 1, 0 }, { 1, 1 } };
        float2 pos1[] = { { 0, 0 }, { 1, 0 }, { 1, -1 } };
        uint16_t indices[] = { 0, 2, 1 };
        m_drawColorTask[0].init({ .device = m_device.Get(),
                                  .color = color,
                                  .posData = pos0,
                                  .numVerts = NumVerts,
                                  .indicesData = indices,
                                  .numIndices = 3,
                                  .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                  .colorAttachmentFormat = m_colorAttachmentFormat,
                                  .depthStencilFormat = m_depthStencilAttachmentFormat });
        m_drawColorTask[1].init({ .device = m_device.Get(),
                                  .color = color,
                                  .posData = pos1,
                                  .numVerts = NumVerts,
                                  .primitiveTopology = PrimitiveTopology::eTriangleStrip,
                                  .colorAttachmentFormat = m_colorAttachmentFormat,
                                  .depthStencilFormat = m_depthStencilAttachmentFormat });
        beginTransfer();
        m_drawColorTask[0].uploadStagingBuffers(m_copyCommandList.Get());
        m_drawColorTask[1].uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask[0].draw(commandList);
        m_drawColorTask[1].draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    DrawColorTask1 m_drawColorTask[2];
    static constexpr uint32_t NumVerts = 3, NumIndices = 3;
};

class TestBarriersBindVbBindVbIb : public TestBarriersBindVbIbBindVb
{
public:
    TestBarriersBindVbBindVbIb(GraphicsBackend backend, UnitTest::Config config)
        : TestBarriersBindVbIbBindVb(backend, config, "test_barriers_bind_vb_bind_vb_ib")
    {
    }

protected:
    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        beginRenderPass(commandList);
        m_drawColorTask[1].draw(commandList);
        m_drawColorTask[0].draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }
};

class TestBarriersUpdateTextureSample : public UnitTest
{
public:
    TestBarriersUpdateTextureSample(GraphicsBackend backend, UnitTest::Config config)
        : UnitTest(backend, config, "test_barriers_update_texture_sample")
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
        m_texture.init(m_device.Get(),
                       { .heapFlags = {},
                         .w = 4,
                         .h = 4,
                         .d = 1,
                         .mipLevels = {},
                         .arrayLayers = 1,
                         .numPlanes = 1,
                         .numSamples = SampleCount::e1,
                         .format = Format::eRgba8Unorm,
                         .rSwizzle = {},
                         .gSwizzle = ComponentSwizzle::eG,
                         .bSwizzle = ComponentSwizzle::eB,
                         .aSwizzle = ComponentSwizzle::eA,
                         .usage = TextureUsage_Sampled,
                         .type = TextureType::e2D,
                         .name = {},
                         .automaticBarriers = true });
        m_drawTextureTask.init({ .device = m_device.Get(),
                                 .texture = &m_texture,
                                 .posData = {},
                                 .numVerts = {},
                                 .indicesData = {},
                                 .numIndices = {},
                                 .uvData = {},
                                 .samplerDesc = {},
                                 .colorAttachmentFormat = m_colorAttachmentFormat,
                                 .depthStencilFormat = {},
                                 .subresource = {} });
        beginTransfer();
        m_drawTextureTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        uint32_t data[] = { 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0x00FF00FF, 0xFF0000FF, 0x00FF00FF,
                            0xFF00FFFF, 0xFF0000FF, 0xFF0000FF, 0xFF00FFFF, 0xFF0000FF, 0xFFFF00FF, 0xFF0000FF, 0xFF0000FF };
        const auto textureDesc = m_texture.get()->desc();
        uint32_t pitch = textureDesc->w * sizeof(uint32_t), h = textureDesc->h;
        commandList->begin();
        commandList->beginTransferPass();
        commandList->updateTexture(m_texture.get(), 0, 0, data, pitch, pitch * h);
        commandList->endTransferPass();
        beginRenderPass(commandList);
        m_drawTextureTask.draw(commandList);
        endRenderPass(commandList);
        commandList->end();
    }

    Texture1 m_texture;
    DrawTextureTask2D m_drawTextureTask;
};

class TestBarriersUpdateTextureSample2 : public UnitTest
{
public:
    TestBarriersUpdateTextureSample2(GraphicsBackend backend, UnitTest::Config config)
        : UnitTest(backend, config, "test_barriers_update_texture_sample_2")
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
        uint32_t data[16]{};
        m_texture.init(m_device.Get(),
                       { .heapFlags = {},
                         .w = 4,
                         .h = 4,
                         .d = 1,
                         .mipLevels = {},
                         .arrayLayers = 1,
                         .numPlanes = 1,
                         .numSamples = SampleCount::e1,
                         .format = Format::eRgba8Unorm,
                         .rSwizzle = {},
                         .gSwizzle = ComponentSwizzle::eG,
                         .bSwizzle = ComponentSwizzle::eB,
                         .aSwizzle = ComponentSwizzle::eA,
                         .usage = TextureUsage_Sampled,
                         .type = TextureType::e2D,
                         .name = {},
                         .automaticBarriers = true },
                       data);
        m_drawTextureTask.init({ .device = m_device.Get(),
                                 .texture = &m_texture,
                                 .posData = {},
                                 .numVerts = {},
                                 .indicesData = {},
                                 .numIndices = {},
                                 .uvData = {},
                                 .samplerDesc = {},
                                 .colorAttachmentFormat = m_colorAttachmentFormat,
                                 .depthStencilFormat = {},
                                 .subresource = {} });
        beginTransfer();
        m_drawTextureTask.uploadStagingBuffers(m_copyCommandList.Get());
        endTransfer();
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        uint32_t data[] = { 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0x00FF00FF, 0xFF0000FF, 0x00FF00FF,
                            0xFF00FFFF, 0xFF0000FF, 0xFF0000FF, 0xFF00FFFF, 0xFF0000FF, 0xFFFF00FF, 0xFF0000FF, 0xFF0000FF };
        uint32_t dstPitch = m_texture.get()->pitch(), w = m_texture.get()->desc()->w, h = m_texture.get()->desc()->h;
        uint32_t srcPitch = w * sizeof(uint32_t);
        uint8_t* src = (uint8_t*)data;
        uint8_t* dst = (uint8_t*)m_texture.stagingBuffer->map();
        for (uint32_t j = 0; j < h; j++)
        {
            memcpy(dst, src, srcPitch);
            dst += dstPitch;
            src += srcPitch;
        }
        m_texture.stagingBuffer->unmap();
        beginTransfer();
        m_texture.uploadFromStagingBuffer(m_copyCommandList.Get());
        endTransfer();
        commandList->begin();
        auto s1 = m_copyCommandList->getState(m_texture.get());
        if (s1)
        {
            commandList->notifyState(m_texture.get(), *s1);
        }
        beginRenderPass(commandList);
        m_drawTextureTask.draw(commandList);
        auto s2 = commandList->getState(m_texture.get());
        if (s2)
        {
            m_copyCommandList->notifyState(m_texture.get(), *s2);
        }
        endRenderPass(commandList);
        commandList->end();
    }

    Texture1 m_texture;
    DrawTextureTask2D m_drawTextureTask;
};

class TestUpdateBufferBindBufferUav : public UnitTest
{
public:
    TestUpdateBufferBindBufferUav(GraphicsBackend backend, UnitTest::Config config)
        : UnitTest(backend, config, "test_barriers_update_buffer_bind_buffer_uav")
    {
        m_config.outputType = OutputType::eBuffer;
        m_config.bufferSize = BufferSize;
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        m_buffer.initAsUnorderedAccessBuffer(m_device.Get(), "uav", nullptr, BufferSize);
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWStructuredBuffer(0, m_buffer.get(), sizeof(uint32_t)), ShaderStage_Compute, 0, m_descSetLayout, m_descSet);
        Shader1 cs;
        initComputeShader(m_device.Get(), "testBarriers", cs, "ComputeWriteTask3");

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descSetLayout));

        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        commandList->beginTransferPass();
        // upload to buffer
        uint32_t data[16];
        for (uint32_t j = 0; j < 16; j++)
        {
            data[j] = 1 << j;
        }
        commandList->updateBuffer(m_buffer.get(), 0, data, sizeof(data));
        commandList->endTransferPass();
        commandList->beginComputePass();
        commandList->bindComputePipeline(m_computePipeline.Get());
        commandList->bindComputeDescriptorSet(m_descSet.Get());
        // write to buffer from compute shader
        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadGroup);
        commandList->endComputePass();
        commandList->beginTransferPass();
        // download buffer
        commandList->copyBuffer(m_outputBuffer.get(), m_buffer.get());
        commandList->endTransferPass();
        commandList->end();
    }

    ResultCode compareOutputBuffer() override
    {
        ScopedBufferMap bufferMap;
        uint32_t ref[16];
        for (uint32_t j = 0; j < 16; j++)
        {
            ref[j] = (1 << j) * 3;
        }
        uint32_t* data = (uint32_t*)mapOutputBuffer(bufferMap);
        QHWI_ASSERT_MSG(data, "Unable to map buffer");
        for (int i = 0; i < 16; ++i)
        {
            if (data[i] != ref[i])
            {
                return ResultCode::eErrorOther;
            }
        }
        return ResultCode::eSuccess;
    };

    Buffer1 m_buffer;
    qre::RefCountPtr<DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<DescriptorSet> m_descSet;
    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout;
    qre::RefCountPtr<ComputePipeline> m_computePipeline;
    static constexpr uint32_t NumThreadGroups[] = { 1, 1, 1 };
    static constexpr uint32_t NumThreadsPerThreadGroup[] = { 1, 1, 1 };
    static constexpr uint32_t BufferSize = 16 * sizeof(uint32_t);
};

class TestUpdateBufferBindBufferUav2 : public UnitTest
{
public:
    TestUpdateBufferBindBufferUav2(GraphicsBackend backend, UnitTest::Config config)
        : UnitTest(backend, config, "test_barriers_update_buffer_bind_buffer_uav_2")
    {
        m_config.outputType = OutputType::eBuffer;
        m_config.bufferSize = BufferSize;
    }

protected:
    ResultCode init() override
    {
        ResultCode resultCode = UnitTest::init();
        if (resultCode != ResultCode::eSuccess)
        {
            return resultCode;
        }
        m_buffer.initAsUnorderedAccessBuffer(m_device.Get(), "uav", nullptr, BufferSize);
        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addRWStructuredBuffer(0, m_buffer.get(), sizeof(uint32_t)), ShaderStage_Compute, 0, m_descSetLayout, m_descSet);
        Shader1 cs;
        initComputeShader(m_device.Get(), "testBarriers", cs, "ComputeWriteTask3");

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descSetLayout));

        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });
        beginTransfer();
        // upload to buffer
        uint32_t data[16];
        for (uint32_t j = 0; j < 16; j++)
        {
            data[j] = 1 << j;
        }
        m_copyCommandList->updateBuffer(m_buffer.get(), 0, data, sizeof(data));
        endTransfer();
        return ResultCode::eSuccess;
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();
        commandList->beginComputePass();
        commandList->bindComputePipeline(m_computePipeline.Get());
        commandList->bindComputeDescriptorSet(m_descSet.Get());
        // write to buffer from compute shader
        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadGroup);
        commandList->endComputePass();
        commandList->end();
    }

    ResultCode compareOutputBuffer() override
    {
        beginTransfer();
        // download buffer
        m_copyCommandList->copyBuffer(m_outputBuffer.get(), m_buffer.get());
        endTransfer();
        ScopedBufferMap bufferMap;
        uint32_t ref[16];
        for (uint32_t j = 0; j < 16; j++)
        {
            ref[j] = (1 << j) * 3;
        }
        uint32_t* data = (uint32_t*)mapOutputBuffer(bufferMap);
        QHWI_ASSERT_MSG(data, "Unable to map buffer");
        for (int i = 0; i < 16; ++i)
        {
            if (data[i] != ref[i])
            {
                return ResultCode::eErrorOther;
            }
        }
        return ResultCode::eSuccess;
    };

    Buffer1 m_buffer;
    qre::RefCountPtr<DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<DescriptorSet> m_descSet;
    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout;
    qre::RefCountPtr<ComputePipeline> m_computePipeline;
    static constexpr uint32_t NumThreadGroups[] = { 1, 1, 1 };
    static constexpr uint32_t NumThreadsPerThreadGroup[] = { 1, 1, 1 };
    static constexpr uint32_t BufferSize = 16 * sizeof(uint32_t);
};

} // namespace test_barriers

using namespace test_barriers;

UnitTest* create_test_barriers_compute_write_compute_read(GraphicsBackend backend, UnitTest::Config config)
{
    BufferParam buffer[2];
    buffer[0].set(std::make_shared<Buffer1>());
    const uint32_t bufferSize0 = sizeof(ComputeWriteTask0::BufferData) * CompMul(ComputeWriteTask0::NumThreadgroups) * CompMul(ComputeWriteTask0::NumThreadsPerThreadgroup);
    const uint32_t bufferSize1 = sizeof(ComputeWriteTask0::BufferData) * CompMul(ComputeReadTask0::NumThreadgroups) * CompMul(ComputeReadTask0::NumThreadsPerThreadgroup);
    config.outputType = UnitTest::OutputType::eBuffer;
    config.bufferSize = bufferSize1;
    TestBarriers* test = new TestBarriers(backend, config);
    buffer[1].set(test->getOutputBuffer());
    test->setTasks({ std::make_shared<ComputeWriteTask0>(test, buffer[0]), std::make_shared<ComputeReadTask0>(test, buffer[0], sizeof(ComputeWriteTask0::BufferData), buffer[1]) });
    test->onInitResources = [=]() { buffer[0]->initAsUnorderedAccessBuffer(test->getDevice(), nullptr, nullptr, bufferSize0); };
    test->onCompareOutputBuffer = [=]() -> ResultCode
    {
        ScopedBufferMap bufferMap;
        uint32_t* outputData = (uint32_t*)test->mapOutputBuffer(bufferMap);
        uint32_t numValues = (bufferSize1 / sizeof(uint32_t));
        for (uint32_t j = 0; j < numValues; j++)
        {
            if (outputData[j] != j)
            {
                return ResultCode::eErrorOther;
            }
        }

        return ResultCode::eSuccess;
    };
    return test;
}

UnitTest* create_test_barriers_compute_read_compute_write(GraphicsBackend backend, UnitTest::Config config)
{
    BufferParam buffer[2];
    buffer[0].set(std::make_shared<Buffer1>());
    const uint32_t bufferSize0 = sizeof(ComputeWriteTask0::BufferData) * CompMul(ComputeWriteTask0::NumThreadgroups) * CompMul(ComputeWriteTask0::NumThreadsPerThreadgroup);
    const uint32_t bufferSize1 = sizeof(ComputeWriteTask0::BufferData) * CompMul(ComputeReadTask0::NumThreadgroups) * CompMul(ComputeReadTask0::NumThreadsPerThreadgroup);
    config.outputType = UnitTest::OutputType::eBuffer;
    config.bufferSize = bufferSize1;
    TestBarriers* test = new TestBarriers(backend, config);
    buffer[1].set(test->getOutputBuffer());
    test->setTasks({ std::make_shared<ComputeReadTask0>(test, buffer[0], sizeof(ComputeWriteTask0::BufferData), buffer[1]), std::make_shared<ComputeWriteTask0>(test, buffer[0]) });
    test->onInitResources = [=]()
    {
        std::vector<uint32_t> data(bufferSize0 / sizeof(uint32_t));
        for (uint32_t j = 0; j < data.size(); j++)
        {
            data[j] = j * 7;
        }
        buffer[0]->initAsUnorderedAccessBuffer(test->getDevice(), nullptr, data.data(), bufferSize0);
    };
    test->onUploadResources = [=](CommandList* copyCommandList) { buffer[0]->uploadFromStagingBuffer(copyCommandList); };
    test->onCompareOutputBuffer = [=]() -> ResultCode
    {
        //need to confirm that the first task read the buffer before the second task writes to it
        ScopedBufferMap bufferMap;
        uint32_t* outputData = (uint32_t*)test->mapOutputBuffer(bufferMap);
        uint32_t numValues = (bufferSize1 / sizeof(uint32_t));
        for (uint32_t j = 0; j < numValues; j++)
        {
            if (outputData[j] != (j * 7))
            {
                return ResultCode::eErrorOther;
            }
        }
        return ResultCode::eSuccess;
    };
    return test;
}

UnitTest* create_test_barriers_compute_write_draw_read(GraphicsBackend backend, UnitTest::Config config)
{
    auto bPos = std::make_shared<Buffer1>();
    auto bIndex = std::make_shared<Buffer1>();
    TestBarriers* test = new TestBarriers(backend, config);
    test->setTasks({ std::make_shared<ComputeWriteTask1>(test, bIndex), std::make_shared<DrawReadTask0>(test, bPos, bIndex) });
    test->onInitResources = [=]()
    {
        std::vector<float2> pos = { { 0.0f, 0.0f }, { 0.5f, 0.0f }, { -0.5f, 0.0f }, { 0.0f, 0.5f }, { 0.0f, -0.5f } };
        bPos->initAsVertexBuffer(test->getDevice(), nullptr, pos.data(), uint32_t(pos.size() * sizeof(pos[0])));
        BufferDesc desc = { .heapType = HeapType::eDefault,
                            .heapFlags = {},
                            .usage = BufferUsage_IndexBuffer | BufferUsage_UnorderedAccess,
                            .size = sizeof(ComputeWriteTask1::BufferData) * CompMul(ComputeWriteTask1::NumThreadgroups) * CompMul(ComputeWriteTask1::NumThreadsPerThreadgroup),
                            .format = {},
                            .name = {},
                            .automaticBarriers = true };
        bIndex->init(test->getDevice(), desc, nullptr);
    };
    test->onUploadResources = [=](CommandList* copyCommandList) { bPos->uploadFromStagingBuffer(copyCommandList); };
    return test;
}

UnitTest* create_test_barriers_compute_write_draw_sample(GraphicsBackend backend, UnitTest::Config config)
{
    auto texture = std::make_shared<Texture1>();
    TestBarriers* test = new TestBarriers(backend, config);
    test->setTasks({ std::make_shared<ComputeWriteTask2>(test, texture), std::make_shared<DrawSampleTask1>(test, texture, (float)config.w, (float)config.h) });
    test->onInitResources = [=]()
    {
        uint32_t w = 32, h = 32;
        texture->init(test->getDevice(),
                      { .heapFlags = {},
                        .w = w,
                        .h = h,
                        .d = 1,
                        .mipLevels = {},
                        .arrayLayers = 1,
                        .numPlanes = 1,
                        .numSamples = SampleCount::e1,
                        .format = Format::eRgba8Unorm,
                        .rSwizzle = {},
                        .gSwizzle = ComponentSwizzle::eG,
                        .bSwizzle = ComponentSwizzle::eB,
                        .aSwizzle = ComponentSwizzle::eA,
                        .usage = TextureUsage_Sampled | TextureUsage_Storage,
                        .type = TextureType::e2D,
                        .name = {},
                        .automaticBarriers = true },
                      nullptr);
    };
    return test;
}

UnitTest* create_test_barriers_draw_render_compute_read(GraphicsBackend backend, UnitTest::Config config)
{
    std::shared_ptr<Texture1> texture[3] = { std::make_shared<Texture1>(), std::make_shared<Texture1>(), std::make_shared<Texture1>() };
    TestBarriers* test = new TestBarriers(backend, config);
    test->setTasks({ std::make_shared<DrawRenderTask0>(test, texture[0], texture[1]),
                     std::make_shared<ComputeReadTask1>(test, texture[1], texture[2]),
                     std::make_shared<DrawSampleTask0>(test, texture[2]) });
    test->onInitResources = [=]()
    {
        uint32_t w0 = 4, h0 = 4;
        std::vector<uint32_t> data(w0 * h0);
        static const uint3 colors[] = {
            { 255, 255, 0 }, { 255, 128, 0 }, { 128, 255, 0 }, { 64, 255, 0 },   { 128, 64, 255 }, { 64, 64, 128 }, { 128, 128, 255 }, { 255, 128, 64 },
            { 64, 128, 64 }, { 64, 64, 255 }, { 0, 64, 255 },  { 64, 128, 255 }, { 64, 0, 64 },    { 64, 0, 255 },  { 255, 0, 64 },    { 255, 64, 64 }
        };
        for (uint32_t j = 0; j < data.size(); j++)
        {
            uint32_t y = j / 4, x = j % 4;
            uint3 c = colors[y * 4 + x];
            data[j] = (c.x) | (c.y << 8) | (c.z << 16) | (0xFF << 24);
        }
        texture[0]->init(test->getDevice(),
                         { .heapFlags = {},
                           .w = w0,
                           .h = h0,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_Sampled,
                           .type = TextureType::e2D,
                           .name = {},
                           .automaticBarriers = true },
                         data.data());
        uint32_t w1 = 32, h1 = 32;
        texture[1]->init(test->getDevice(),
                         { .heapFlags = {},
                           .w = w1,
                           .h = h1,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_ColorAttachment | TextureUsage_Storage,
                           .type = TextureType::e2D,
                           .name = {},
                           .automaticBarriers = true },
                         nullptr);
        texture[2]->init(test->getDevice(),
                         { .heapFlags = {},
                           .w = w1,
                           .h = h1,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_Storage | TextureUsage_Sampled,
                           .type = TextureType::e2D,
                           .name = {},
                           .automaticBarriers = true },
                         nullptr);
    };
    return test;
}

UnitTest* create_test_barriers_draw_render_draw_sample(GraphicsBackend backend, UnitTest::Config config)
{
    std::shared_ptr<Texture1> texture[2] = { std::make_shared<Texture1>(), std::make_shared<Texture1>() };
    TestBarriers* test = new TestBarriers(backend, config);
    test->setTasks({ std::make_shared<DrawRenderTask0>(test, texture[0], texture[1]), std::make_shared<DrawSampleTask0>(test, texture[1]) });
    test->onInitResources = [=]()
    {
        uint32_t w = 4, h = 4;
        std::vector<uint32_t> data(w * h);
        static const uint3 colors[] = {
            { 128, 64, 255 }, { 64, 64, 128 }, { 128, 128, 255 }, { 255, 128, 64 }, { 255, 255, 0 }, { 255, 128, 0 }, { 128, 255, 0 }, { 64, 255, 0 },
            { 64, 0, 64 },    { 64, 0, 255 },  { 255, 0, 64 },    { 255, 64, 64 },  { 64, 128, 64 }, { 64, 64, 255 }, { 0, 64, 255 },  { 64, 128, 255 }
        };
        for (uint32_t j = 0; j < data.size(); j++)
        {
            uint32_t y = j / 4, x = j % 4;
            uint3 c = colors[y * 4 + x];
            data[j] = (c.z) | (c.x << 8) | (c.y << 16) | (0xFF << 24);
        }
        texture[0]->init(test->getDevice(),
                         { .heapFlags = {},
                           .w = w,
                           .h = h,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_Sampled,
                           .type = TextureType::e2D,
                           .name = {},
                           .automaticBarriers = true },
                         data.data());
        texture[1]->init(test->getDevice(),
                         { .heapFlags = {},
                           .w = w,
                           .h = h,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_Sampled | TextureUsage_ColorAttachment,
                           .type = TextureType::e2D,
                           .name = {},
                           .automaticBarriers = true },
                         nullptr);
    };
    return test;
}

UnitTest* create_test_barriers_draw_sample_draw_render(GraphicsBackend backend, UnitTest::Config config)
{
    std::shared_ptr<Texture1> texture[2] = { std::make_shared<Texture1>(), std::make_shared<Texture1>() };
    TestBarriers* test = new TestBarriers(backend, config);
    test->setTasks({ std::make_shared<DrawSampleTask0>(test, texture[0]), std::make_shared<DrawRenderTask0>(test, texture[1], texture[0]) });
    test->onInitResources = [=]()
    {
        uint32_t w = 4, h = 4;
        std::vector<uint32_t> data0(w * h);
        static const uint3 colors[] = {
            { 255, 255, 0 }, { 255, 128, 0 }, { 128, 255, 0 }, { 64, 255, 0 },   { 64, 0, 64 },    { 64, 0, 255 },  { 255, 0, 64 },    { 255, 64, 64 },
            { 64, 128, 64 }, { 64, 64, 255 }, { 0, 64, 255 },  { 64, 128, 255 }, { 128, 64, 255 }, { 64, 64, 128 }, { 128, 128, 255 }, { 255, 128, 64 }
        };
        for (uint32_t j = 0; j < data0.size(); j++)
        {
            uint32_t y = j / 4, x = j % 4;
            uint3 c = colors[y * 4 + x];
            data0[j] = (c.z) | (c.x << 8) | (c.y << 16) | (0xFF << 24);
        }
        texture[0]->init(test->getDevice(),
                         { .heapFlags = {},
                           .w = w,
                           .h = h,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_Sampled | TextureUsage_ColorAttachment,
                           .type = TextureType::e2D,
                           .name = "texture0",
                           .automaticBarriers = true },
                         data0.data());
        std::vector<uint32_t> data1(w * h);
        for (uint32_t j = 0; j < data1.size(); j++)
        {
            uint32_t y = j / 4, x = j % 4;
            uint3 c = colors[x * 4 + y];
            data1[j] = (c.z) | (c.x << 8) | (c.y << 16) | (0xFF << 24);
        }
        texture[1]->init(test->getDevice(),
                         { .heapFlags = {},
                           .w = w,
                           .h = h,
                           .d = 1,
                           .mipLevels = {},
                           .arrayLayers = 1,
                           .numPlanes = 1,
                           .numSamples = SampleCount::e1,
                           .format = Format::eRgba8Unorm,
                           .rSwizzle = {},
                           .gSwizzle = ComponentSwizzle::eG,
                           .bSwizzle = ComponentSwizzle::eB,
                           .aSwizzle = ComponentSwizzle::eA,
                           .usage = TextureUsage_Sampled,
                           .type = TextureType::e2D,
                           .name = "texture1",
                           .automaticBarriers = true },
                         data1.data());
    };
    return test;
}

UnitTest* create_test_barriers_bind_vb_update(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestBarriersBindVbUpdate(backend, config);
}

UnitTest* create_test_barriers_bind_vb_ib_bind_vb(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestBarriersBindVbIbBindVb(backend, config);
}

UnitTest* create_test_barriers_bind_vb_bind_vb_ib(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestBarriersBindVbBindVbIb(backend, config);
}

UnitTest* create_test_barriers_update_texture_sample(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestBarriersUpdateTextureSample(backend, config);
}

UnitTest* create_test_barriers_update_texture_sample_2(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestBarriersUpdateTextureSample2(backend, config);
}

UnitTest* create_test_barriers_update_buffer_bind_buffer_uav(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestUpdateBufferBindBufferUav(backend, config);
}

UnitTest* create_test_barriers_update_buffer_bind_buffer_uav_2(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestUpdateBufferBindBufferUav2(backend, config);
}
