//
//  test_ray_pipeline_sphere.cpp
//  QHWI
//
// Copyright(c) Qualcomm Technologies, Inc. and /or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_ray_pipeline_sphere.h"
#include <qre/math/vecmath.h>
#include <qre/math/float4.h>

TestRayPipelineSphere::TestRayPipelineSphere(GraphicsBackend graphicsBackend, Config config, const char* name)
    : TestRayPipeline(graphicsBackend, config, name)
    , m_R(0.4f * static_cast<float>(std::min(config.w, config.h)))
{
}

void TestRayPipelineSphere::initGeometry()
{
    //initialize positions
    float3 pos[NumVerts]{};
    float3* posPtr = pos;

    for (uint32_t lat = 0; lat <= NumLatitude; ++lat)
    {
        float theta = float(lat) * float(M_PI) / float(NumLatitude) - float(M_PI) / 2.0f;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        for (uint32_t lon = 0; lon <= NumLongitude; ++lon)
        {
            float phi = float(lon) * 2.0f * float(M_PI) / float(NumLongitude);
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);

            float x = m_R * cosTheta * cosPhi;
            float z = m_R * cosTheta * sinPhi;
            float y = m_R * sinTheta;

            *posPtr++ = { x, y, z };
        }
    }

    //initialize indices
    uint16_t indices[NumIndices]{};
    uint16_t* indexPtr = indices;
    for (uint16_t lat = 0; lat < NumLatitude; ++lat)
    {
        for (uint16_t lon = 0; lon < NumLongitude; ++lon)
        {
            uint16_t first = (lat * (NumLongitude + 1)) + lon;
            uint16_t second = first + NumLongitude + 1;

            *indexPtr++ = first;
            *indexPtr++ = second;
            *indexPtr++ = first + 1;

            *indexPtr++ = second;
            *indexPtr++ = second + 1;
            *indexPtr++ = first + 1;
        }
    }

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

void TestRayPipelineSphere::initInstanceData()
{
    const float translation_x = static_cast<float>(m_width) * 0.5f;
    const float translation_y = static_cast<float>(m_height) * 0.5f;

    m_instanceData[0].transform[0] = 1.0f;
    m_instanceData[0].transform[1] = 0.0f;
    m_instanceData[0].transform[2] = 0.0f;
    m_instanceData[0].transform[3] = translation_x;

    m_instanceData[0].transform[4] = 0.0f;
    m_instanceData[0].transform[5] = 1.0f;
    m_instanceData[0].transform[6] = 0.0f;
    m_instanceData[0].transform[7] = translation_y;

    m_instanceData[0].transform[8] = 0.0f;
    m_instanceData[0].transform[9] = 0.0f;
    m_instanceData[0].transform[10] = 1.0f;
    m_instanceData[0].transform[11] = 0.0f;

    m_instanceData[0].instanceMask = 0xFF;
    m_instanceData[0].instanceID = 0;
    m_instanceData[0].instanceContributionToHitGroupIndex = 0;
    m_instanceData[0].blasIndex = 0;
    m_instanceData[0].flags = 0;
}

void TestRayPipelineSphere::initAccelerationStructures()
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

    AccelerationStructureDesc blasDesc = { .isTopLevel = false, .allocationSize = blasBuildInfo.accelerationStructureSize, .name = nullptr };

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

    qhwi::AccelerationStructure* blasPtr(m_blas.Get());
    m_instanceBuffer = m_device->createTopLevelInstanceBuffer(m_instanceData, &blasPtr, NumInstances);

    AccelerationStructureBuildInfo tlasBuildInfo = m_device->getAccelerationStructureBuildInfo(NumInstances);

    AccelerationStructureDesc tlasDesc = { .isTopLevel = true, .allocationSize = tlasBuildInfo.accelerationStructureSize, .name = nullptr };

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
    cmdList->beginAccelerationStructurePass();
    cmdList->buildBottomLevelAccelerationStructure(m_blas.Get(), blasScratchBuffer.Get(), &bottomLevelDesc, 1, bottomLevelScratchMemory);
    cmdList->endAccelerationStructurePass();
    cmdList->end();

    m_commandQueue->executeCommandList(cmdList.Get());
    m_commandQueue->waitForIdle();

    free(bottomLevelScratchMemory);
}

void TestRayPipelineSphere::recordCommandList(CommandList* commandList)
{
    commandList->begin();

    // TLAS
    AccelerationStructure* blasArray[] = { m_blas.Get() };
    commandList->beginAccelerationStructurePass();
    commandList->buildTopLevelAccelerationStructure(m_tlas.Get(), m_instanceBuffer.Get(), m_tlasScratchBuffer.Get(), NumInstances, blasArray, 1);
    commandList->endAccelerationStructurePass();

    commandList->addBufferBarrier({ .buffer = m_tlasScratchBuffer.Get(), .src = BufferState_ComputeShader_UnorderedAccess, .dst = BufferState_ComputeShader_UnorderedAccess });

    commandList->beginComputePass();
    commandList->bindRayTracingPipeline(m_rayTracingPipeline);
    commandList->bindRayTracingDescriptorSet(m_descriptorSets[0].Get());
    commandList->dispatchRays(m_rayGenShaderBindingTable.shaderTable,
                              m_missShaderBindingTable.shaderTable,
                              m_hitShaderBindingTable.shaderTable,
                              StridedDeviceAddressRegion(),
                              m_width,
                              m_height,
                              1);
    commandList->endComputePass();

    // Blit
    beginRenderPass(commandList);
    m_drawTextureTask.draw(commandList);
    endRenderPass(commandList);

    commandList->end();
}

UnitTest* create_test_ray_pipeline_sphere(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestRayPipelineSphere(backend, config, "test_ray_pipeline_sphere");
}
