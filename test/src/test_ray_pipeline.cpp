//
//  test_ray_pipeline.cpp
//  QHWI
//
// Copyright(c) Qualcomm Technologies, Inc. and /or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "test_ray_pipeline.h"
#include <qre/math/float4.h>
#include <qre/math/vecmath.h>

TestRayPipeline::TestRayPipeline(GraphicsBackend graphicsBackend, Config config, const char* name, const bool enableIndirectTraceRays)
    : UnitTest(graphicsBackend, config, name)
    , m_width(config.w)
    , m_height(config.h)
{
}

ResultCode TestRayPipeline::init()
{
    ResultCode resultCode = UnitTest::init();
    if (resultCode != ResultCode::eSuccess)
    {
        return resultCode;
    }

    if (!m_device->supportsRayPipelines())
    {
        return ResultCode::eUnsupported;
    }

    initGeometry();
    initInstanceData();
    uploadVertexData();
    initAccelerationStructures();

    initShaders();
    initRayPipeline();
    createShaderBindingTables();

    return ResultCode::eSuccess;
}

void TestRayPipeline::initGeometry()
{
    const float l = static_cast<float>(m_height) * 0.15f;

    // Equilateral triangle of side length l
    //initialize positions
    float3 pos[NumVerts] = {
        { 0.0f, 0.5774f * l, 0.0f },       // Top vertex
        { -0.5f * l, -0.2887f * l, 0.0f }, // Bottom-left
        { 0.5f * l, -0.2887f * l, 0.0f }   // Bottom-right
    };

    //initialize indices
    uint16_t indices[NumIndices] = { 0, 1, 2 };

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

void TestRayPipeline::initInstanceData()
{
    const float w = static_cast<float>(m_width);
    const float h = static_cast<float>(m_height);
    const std::vector<qre::math::Vec3f> translations = { { 0.5f * w, 0.5f * h, 20.0f },
                                                         { 0.25f * w, 0.25f * h, 25.0f },
                                                         { 0.75f * w, 0.25f * h, 25.0f },
                                                         { 0.25f * w, 0.75f * h, 15.0f },
                                                         { 0.75f * w, 0.75f * h, 15.0f } };

    for (uint32_t idx = 0; idx < NumInstances; idx++)
    {
        m_instanceData[idx].transform[0] = 1.0f;
        m_instanceData[idx].transform[1] = 0.0f;
        m_instanceData[idx].transform[2] = 0.0f;
        m_instanceData[idx].transform[3] = translations[idx].x;
        m_instanceData[idx].transform[4] = 0.0f;
        m_instanceData[idx].transform[5] = 1.0f;
        m_instanceData[idx].transform[6] = 0.0f;
        m_instanceData[idx].transform[7] = translations[idx].y;
        m_instanceData[idx].transform[8] = 0.0f;
        m_instanceData[idx].transform[9] = 0.0f;
        m_instanceData[idx].transform[10] = 1.0f;
        m_instanceData[idx].transform[11] = translations[idx].z;
        m_instanceData[idx].instanceMask = 0xFF;
        m_instanceData[idx].instanceID = idx;
        m_instanceData[idx].instanceContributionToHitGroupIndex = 0;
        m_instanceData[idx].blasIndex = 0;
        m_instanceData[idx].flags = 0;
    }
}

void TestRayPipeline::uploadVertexData()
{
    qre::RefCountPtr<CommandList> cmdList = m_device->createCommandList(m_commandQueue.Get(), { .type = CommandListType::ePrimary, .supportsAutomaticBarriers = true });
    cmdList->begin();
    cmdList->beginTransferPass();
    m_vertexBuffer.uploadFromStagingBuffer(cmdList.Get());
    m_indexBuffer.uploadFromStagingBuffer(cmdList.Get());
    cmdList->endTransferPass();
    cmdList->end();
    m_commandQueue->executeCommandList(cmdList.Get());
    m_commandQueue->waitForIdle();
}

void TestRayPipeline::initAccelerationStructures()
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

    qhwi::AccelerationStructure* blasPtr(m_blas.Get());
    m_instanceBuffer = m_device->createTopLevelInstanceBuffer(m_instanceData, &blasPtr, NumInstances);

    AccelerationStructureBuildInfo tlasBuildInfo = m_device->getAccelerationStructureBuildInfo(NumInstances);

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
    cmdList->beginAccelerationStructurePass();
    cmdList->buildBottomLevelAccelerationStructure(m_blas.Get(), blasScratchBuffer.Get(), &bottomLevelDesc, 1, bottomLevelScratchMemory);
    cmdList->endAccelerationStructurePass();
    cmdList->end();

    m_commandQueue->executeCommandList(cmdList.Get());
    m_commandQueue->waitForIdle();

    free(bottomLevelScratchMemory);
}

void TestRayPipeline::initShaders()
{
    // Init shaders
    initShader(m_device.Get(), "testRayPipeline", "RayGeneration_Main", qhwi::ShaderType::eRayGeneration, shaders1[int(RTPipelineShaders::RayGen)]);
    shaders.push_back(shaders1[int(RTPipelineShaders::RayGen)].shader);

    initShader(m_device.Get(), "testRayPipeline", "Miss_Main", qhwi::ShaderType::eMiss, shaders1[int(RTPipelineShaders::Miss)]);
    shaders.push_back(shaders1[int(RTPipelineShaders::Miss)].shader);

    initShader(m_device.Get(), "testRayPipeline", "ClosestHit_Main", qhwi::ShaderType::eClosestHit, shaders1[int(RTPipelineShaders::ClosestHit)]);
    shaders.push_back(shaders1[int(RTPipelineShaders::ClosestHit)].shader);

    // Create shader groups
    shaderGroups.resize(int(RTPipelineShaders::Count));
    shaderGroups[int(int(RTPipelineShaders::RayGen))] = RayTracingPipelineShaderGroup()
                                                            .setGeneralShader(int(RTPipelineShaders::RayGen))
                                                            .setShaderGroupType(RayTracingShaderGroupType::eGeneral)
                                                            .setShaderGroupName("shaderGroup_raygen");

    shaderGroups[int(int(RTPipelineShaders::Miss))] = RayTracingPipelineShaderGroup()
                                                          .setGeneralShader(int(RTPipelineShaders::Miss))
                                                          .setShaderGroupType(RayTracingShaderGroupType::eGeneral)
                                                          .setShaderGroupName("shaderGroup_miss");

    shaderGroups[int(int(RTPipelineShaders::ClosestHit))] = RayTracingPipelineShaderGroup()
                                                                .setClosestHitShader(int(RTPipelineShaders::ClosestHit))
                                                                .setShaderGroupType(RayTracingShaderGroupType::eTriangleHit)
                                                                .setShaderGroupName("shaderGroup_hit");
}

void TestRayPipeline::initRayPipeline()
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

    qre::RefCountPtr<CommandList> cmdList = m_device->createCommandList(m_commandQueue.Get(), { .type = CommandListType::ePrimary, .supportsAutomaticBarriers = true });

    cmdList->begin();
    cmdList->beginTransferPass();
    m_drawTextureTask.uploadStagingBuffers(cmdList.Get());
    cmdList->endTransferPass();
    cmdList->end();

    m_commandQueue->executeCommandList(cmdList.Get());
    m_commandQueue->waitForIdle();

    // Create descriptor layout
    m_device->createDescriptorSetAndLayout(DescriptorSetDesc().addAccelerationStructure(0, m_tlas).addRWTexture(0, m_outputTexture.texture),
                                           ShaderStage_RayGeneration | ShaderStage_ClosestHit | ShaderStage_Miss,
                                           0,
                                           m_descriptorSetLayouts[0],
                                           m_descriptorSets[0]);

    m_pipelineLayout = m_device->createPipelineLayout(PipelineLayoutDesc().addDescriptorSetLayout(m_descriptorSetLayouts[0]).setName("rayPipeline_PipelineLayout"));

    m_rayTracingPipeline = m_device->createRayTracingPipeline(RayTracingPipelineDesc()
                                                                  .setLayout(m_pipelineLayout)
                                                                  .setShaders({ shaders.data(), shaders.size() })
                                                                  .setShaderGroups(shaderGroups)
                                                                  .setName("rayTracingPipeline")
                                                                  .setMaxPayloadSize(sizeof(qre::math::Vec4f))
                                                                  .setMaxAttributeSize(sizeof(qre::math::Vec2f)));
}

void TestRayPipeline::createShaderBindingTables()
{
    // Get shader handles
    constexpr uint32_t NumShaderGroups = uint32_t(RTPipelineShaders::Count);
    constexpr uint32_t ScratchBufferSize = 512;
    const RayTracingPipelineProperties rtProperties = m_device->getRayTracingPipelineProperties();
    uint8_t scratchBuffer[ScratchBufferSize];

    m_device->getRayTracingShaderGroupHandles(m_rayTracingPipeline, 0, NumShaderGroups, ScratchBufferSize, scratchBuffer);

    BufferDesc bufferDesc =
        BufferDesc().setAutomaticBarriers(true).setHeapType(HeapType::eUpload).setUsage(BufferUsage_ShaderBindingTable).setSize(rtProperties.shaderGroupHandleSize);

    // Create shader binding tables
    {
        bufferDesc.setName("sbtRayGenShader");
        m_rayGenShaderBindingTable.init(m_device.Get(), bufferDesc, scratchBuffer, rtProperties);

        bufferDesc.setName("sbtMissShader");
        m_missShaderBindingTable.init(m_device.Get(), bufferDesc, scratchBuffer + rtProperties.shaderGroupHandleSize * int(RTPipelineShaders::Miss), rtProperties);

        bufferDesc.setName("sbtHitShader");
        m_hitShaderBindingTable.init(m_device.Get(), bufferDesc, scratchBuffer + rtProperties.shaderGroupHandleSize * int(RTPipelineShaders::ClosestHit), rtProperties);
    }
    if (m_enableIndirectTraceRays)
    {
        m_unifiedTraceRaysIndirectCommand = { .raygenAddress = m_rayGenShaderBindingTable.shaderTable.startAddress,
                                              .raygenSize = m_rayGenShaderBindingTable.shaderTable.size,

                                              .missTableAddress = m_missShaderBindingTable.shaderTable.startAddress,
                                              .missTableSize = m_missShaderBindingTable.shaderTable.size,
                                              .missTableStride = m_missShaderBindingTable.shaderTable.stride,

                                              .hitTableAddress = m_hitShaderBindingTable.shaderTable.startAddress,
                                              .hitTableSize = m_hitShaderBindingTable.shaderTable.size,
                                              .hitTableStride = m_hitShaderBindingTable.shaderTable.stride,

                                              .callableTableAddress = 0,
                                              .callableTableSize = 0,
                                              .callableTableStride = 0,

                                              .width = m_width,
                                              .height = m_height,
                                              .depth = 1 };

        BufferDesc ibDesc = { .heapType = HeapType::eDefault,
                              .heapFlags = {},
                              .usage = BufferUsage_IndirectBuffer,
                              .size = sizeof(m_unifiedTraceRaysIndirectCommand),
                              .format = {},
                              .name = "IndirectBuffer",
                              .automaticBarriers = false };

        m_indirectBuffer.init(m_device.Get(), ibDesc, &m_unifiedTraceRaysIndirectCommand);

        qre::RefCountPtr<CommandList> cmdList = m_device->createCommandList(m_commandQueue.Get(), { .type = CommandListType::ePrimary, .supportsAutomaticBarriers = true });
        cmdList->begin();
        cmdList->beginTransferPass();
        m_indirectBuffer.uploadFromStagingBuffer(cmdList.Get());
        cmdList->endTransferPass();
        cmdList->end();
        m_commandQueue->executeCommandList(cmdList.Get());
        m_commandQueue->waitForIdle();
    }
}

void TestRayPipeline::recordCommandList(CommandList* commandList)
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
    if (m_enableIndirectTraceRays == false)
    {
        commandList->dispatchRays(m_rayGenShaderBindingTable.shaderTable,
                                  m_missShaderBindingTable.shaderTable,
                                  m_hitShaderBindingTable.shaderTable,
                                  StridedDeviceAddressRegion(),
                                  m_width,
                                  m_height,
                                  1);
    }
    else
    {
        commandList->dispatchRaysIndirect(m_indirectBuffer.get());
    }
    commandList->endComputePass();

    // Blit
    beginRenderPass(commandList);
    m_drawTextureTask.draw(commandList);
    endRenderPass(commandList);

    commandList->end();
}

UnitTest* create_test_ray_pipeline(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestRayPipeline(backend, config, "test_ray_pipeline", false);
}

UnitTest* create_test_ray_pipeline_indirect(GraphicsBackend backend, UnitTest::Config config)
{
    return new TestRayPipeline(backend, config, "test_ray_pipeline_indirect", true);
}
