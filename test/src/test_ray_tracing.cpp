//
//
//  test_ray_tracing.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_util.h"
#include "unit_test.h"
using namespace qhwi;
using namespace test_util;

class TestRayTracing : public UnitTest
{
public:
    TestRayTracing(GraphicsBackend graphicsBackend, Config config, const char* name = "test_ray_tracing")
        : UnitTest(graphicsBackend, config, name)
        , m_width(config.w)
        , m_height(config.h)
        , m_R(0.4f * static_cast<float>(std::min(config.w, config.h)))
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

        if (!m_device->supportsRayTracing())
        {
            return ResultCode::eUnsupported;
        }

        initGeometry();
        initAccelerationStructures();
        initCompute();

        return ResultCode::eSuccess;
    }

    void initGeometry()
    {
        //initialize positions
        float3 pos[NumVerts];
        float3* posPtr = pos;
        const float c_x = static_cast<float>(m_width) * 0.5f;
        const float c_y = static_cast<float>(m_height) * 0.5f;
        *posPtr++ = { c_x, c_y, 0.5f };

        for (int j = 1; j < NumVerts; j++)
        {
            float theta = 2.0f * float(M_PI) * (float(j - 1) / float(NumVerts - 1));
            *posPtr++ = { c_x + m_R * cosf(theta), c_y + m_R * sinf(theta), 0.5f };
        }

        //initialize indices
        uint16_t indices[NumIndices];
        uint16_t* indexPtr = indices;
        for (int j = 0; j < (NumTriangles - 1); j++)
        {
            *indexPtr++ = uint16_t(0);
            *indexPtr++ = uint16_t(j + 2);
            *indexPtr++ = uint16_t(j + 1);
        }
        *indexPtr++ = 0;
        *indexPtr++ = 1;
        *indexPtr++ = NumVerts - 1;

        BufferDesc vbDesc = { .heapType = HeapType::eDefault,
                              .heapFlags = {},
                              .usage = BufferUsage_ConstantBuffer | BufferUsage_AccelerationStructureBuildSource,
                              .size = sizeof(pos),
                              .format = {},
                              .name = "VertexBuffer",
                              .automaticBarriers = true };

        m_vertexBuffer.init(m_device.Get(), vbDesc, pos);

        BufferDesc ibDesc = { .heapType = HeapType::eDefault,
                              .heapFlags = {},
                              .usage = BufferUsage_ConstantBuffer | BufferUsage_AccelerationStructureBuildSource,
                              .size = sizeof(indices),
                              .format = {},
                              .name = "IndexBuffer",
                              .automaticBarriers = true };

        m_indexBuffer.init(m_device.Get(), ibDesc, indices);
    }

    void initAccelerationStructures()
    {
        // BLAS descriptor
        BottomLevelDescriptor bottomLevelDesc;
        bottomLevelDesc.contentsType = BottomLevelDescriptor::ContentsType::Triangles;
        bottomLevelDesc.triangles = { .vertexBuffer = m_vertexBuffer.get(),
                                      .vertexOffset = 0,
                                      .vertexStride = 3 * sizeof(float),
                                      .vertexCount = NumVerts,
                                      .vertexFormat = Format::eRgb32Float,
                                      .indexBuffer = m_indexBuffer.get(),
                                      .indexOffset = 0,
                                      .indexCount = NumIndices,
                                      .indexFormat = IndexFormat::eUint16 };

        void* bottomLevelScratchMemory = malloc(m_device->getBottomLevelScratchSize(1));

        // Figure out buffer sizes
        AccelerationStructureBuildInfo blasBuildInfo = m_device->getAccelerationStructureBuildInfo(&bottomLevelDesc, 1, bottomLevelScratchMemory);

        AccelerationStructureDesc blasDesc = { .isTopLevel = {}, .allocationSize = blasBuildInfo.accelerationStructureSize, .name = {} };

        m_blas = m_device->createAccelerationStructure(blasDesc);

        // BLAS scratch buffer
        BufferDesc blasScratchDesc = { .heapType = HeapType::eDefault,
                                       .heapFlags = {},
                                       .usage = BufferUsage_UnorderedAccess,
                                       .size = blasBuildInfo.buildScratchSize,
                                       .format = {},
                                       .name = "BlasScratchBuffer",
                                       .automaticBarriers = true };

        qre::RefCountPtr<Buffer> blasScratchBuffer = m_device->createBuffer(blasScratchDesc);

        TopLevelDescriptor instanceData[1];
        instanceData[0].transform[0] = 1.0f;
        instanceData[0].transform[1] = 0.0f;
        instanceData[0].transform[2] = 0.0f;
        instanceData[0].transform[3] = 0.0f;
        instanceData[0].transform[4] = 0.0f;
        instanceData[0].transform[5] = 1.0f;
        instanceData[0].transform[6] = 0.0f;
        instanceData[0].transform[7] = 0.0f;
        instanceData[0].transform[8] = 0.0f;
        instanceData[0].transform[9] = 0.0f;
        instanceData[0].transform[10] = 1.0f;
        instanceData[0].transform[11] = 1.0f;
        instanceData[0].instanceMask = 0xFF;
        instanceData[0].instanceID = 0;
        instanceData[0].instanceContributionToHitGroupIndex = 0;
        instanceData[0].blasIndex = 0;
        instanceData[0].flags = 0;

        qhwi::AccelerationStructure* blasPtr(m_blas.Get());
        m_instanceBuffer = m_device->createTopLevelInstanceBuffer(instanceData, &blasPtr, 1);

        AccelerationStructureBuildInfo tlasBuildInfo = m_device->getAccelerationStructureBuildInfo(1);

        AccelerationStructureDesc tlasDesc = { .isTopLevel = true, .allocationSize = tlasBuildInfo.accelerationStructureSize, .name = {} };

        m_tlas = m_device->createAccelerationStructure(tlasDesc);

        // TLAS scratch buffer
        BufferDesc tlasScratchDesc = { .heapType = HeapType::eDefault,
                                       .heapFlags = {},
                                       .usage = BufferUsage_UnorderedAccess,
                                       .size = tlasBuildInfo.buildScratchSize,
                                       .format = {},
                                       .name = "TlasScratchBuffer",
                                       .automaticBarriers = true };

        m_tlasScratchBuffer = m_device->createBuffer(tlasScratchDesc);

        // Build BLAS
        qre::RefCountPtr<CommandList> cmdList = m_device->createCommandList(m_commandQueue.Get(), { .type = CommandListType::ePrimary, .supportsAutomaticBarriers = true });

        cmdList->begin();

        // BLAS

        cmdList->beginTransferPass();
        m_vertexBuffer.uploadFromStagingBuffer(cmdList.Get());
        m_indexBuffer.uploadFromStagingBuffer(cmdList.Get());
        cmdList->endTransferPass();

        cmdList->beginAccelerationStructurePass();
        cmdList->buildBottomLevelAccelerationStructure(m_blas.Get(), blasScratchBuffer.Get(), &bottomLevelDesc, 1, bottomLevelScratchMemory);
        cmdList->endAccelerationStructurePass();

        cmdList->end();

        m_commandQueue->executeCommandList(cmdList.Get());
        m_commandQueue->waitForIdle();

        free(bottomLevelScratchMemory);
    }

    void initCompute()
    {
        // Result buffer to copy UAV contents to
        {
            qhwi::TextureDesc desc = { .heapFlags = {},
                                       .w = m_width,
                                       .h = m_height,
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
                                       .automaticBarriers = true };

            m_outputTexture.init(m_device.Get(), desc, nullptr);

            m_drawTextureTask.init({ .device = m_device.Get(),
                                     .texture = &m_outputTexture,
                                     .posData = {},
                                     .numVerts = {},
                                     .indicesData = {},
                                     .numIndices = {},
                                     .uvData = {},
                                     .samplerDesc = {},
                                     .colorAttachmentFormat = m_colorAttachmentFormat,
                                     .depthStencilFormat = {},
                                     .subresource = {} });
        }

        m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addAccelerationStructure(0, m_tlas).addRWTexture(0, m_outputTexture.texture),
                                               ShaderStage_Compute,
                                               0,
                                               m_descriptorSetLayouts[0],
                                               m_descriptorSets[0]);

        m_computePipelineLayout = m_device->create(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]));
        Shader1 cs;
        initComputeShader(m_device.Get(), "testRayTracing", cs);
        m_computePipeline = m_device->createComputePipeline({ .computeShader = cs.shader, .layout = m_computePipelineLayout });

        qre::RefCountPtr<CommandList> cmdList = m_device->createCommandList(m_commandQueue.Get(), { .type = CommandListType::ePrimary, .supportsAutomaticBarriers = true });

        cmdList->begin();
        cmdList->beginTransferPass();
        m_drawTextureTask.uploadStagingBuffers(cmdList.Get());
        cmdList->endTransferPass();
        cmdList->end();

        m_commandQueue->executeCommandList(cmdList.Get());
        m_commandQueue->waitForIdle();
    }

    void recordCommandList(CommandList* commandList) override
    {
        commandList->begin();

        // TLAS
        AccelerationStructure* blasArray[] = { m_blas.Get() };
        commandList->beginAccelerationStructurePass();
        commandList->buildTopLevelAccelerationStructure(m_tlas.Get(), m_instanceBuffer.Get(), m_tlasScratchBuffer.Get(), 1, blasArray, 1);
        commandList->endAccelerationStructurePass();

        commandList->addBufferBarrier({ .buffer = m_tlasScratchBuffer.Get(), .src = BufferState_ComputeShader_UnorderedAccess, .dst = BufferState_ComputeShader_UnorderedAccess });

        // Ray query
        commandList->beginComputePass();
        commandList->bindComputePipeline(m_computePipeline.Get());
        commandList->bindComputeDescriptorSet(m_descriptorSets[0].Get());
        const uint32_t NumThreadGroups[] = { m_width / NumThreadsPerThreadGroup[0], m_height / NumThreadsPerThreadGroup[1], NumThreadsPerThreadGroup[2] };
        commandList->dispatch(NumThreadGroups, NumThreadsPerThreadGroup);
        commandList->endComputePass();

        // Blit
        beginRenderPass(commandList);
        m_drawTextureTask.draw(commandList);
        endRenderPass(commandList);

        commandList->end();
    }

protected:
    static constexpr int NumTriangles = 63;
    static constexpr int NumVerts = NumTriangles + 1;
    static constexpr int NumIndices = NumTriangles * 3;

    Buffer1 m_vertexBuffer;
    Buffer1 m_indexBuffer;
    qre::RefCountPtr<qhwi::Buffer> m_instanceBuffer;
    qre::RefCountPtr<qhwi::AccelerationStructure> m_blas;
    qre::RefCountPtr<qhwi::AccelerationStructure> m_tlas;
    qre::RefCountPtr<qhwi::Buffer> m_tlasScratchBuffer;

    uint32_t m_width;
    uint32_t m_height;
    float m_R;

    qre::RefCountPtr<PipelineLayout> m_computePipelineLayout;
    qre::RefCountPtr<ComputePipeline> m_computePipeline;

    static const int NumDescriptorSetLayouts = 1;
    static const int NumDescriptorSets = 1;
    qre::RefCountPtr<DescriptorSetLayout> m_descriptorSetLayouts[NumDescriptorSetLayouts];
    qre::RefCountPtr<DescriptorSet> m_descriptorSets[NumDescriptorSets];
    Texture1 m_outputTexture;
    DrawTextureTask2D m_drawTextureTask;
    static constexpr uint32_t NumThreadsPerThreadGroup[] = { 16, 4, 1 };
};

UnitTest* create_test_ray_tracing(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestRayTracing(backend, config);
}
