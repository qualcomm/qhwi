//
//
//  qhwi_vk_enums.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "qhwi/qhwi_vk_enums.h"
#include "qhwi_vk_enums_internal.h"
#include "qhwi/qhwi_vk.h"

#define CASE(p0, p1)                                                                                                                                                               \
    case p0:                                                                                                                                                                       \
        return p1

#define VK_CASE(p0)                                                                                                                                                                \
    case p0:                                                                                                                                                                       \
        return vk::p0

#define VK_SAMPLE_COUNT(n) CASE(SampleCount::e##n, vk::SampleCountFlagBits::e##n)

#define VK_FLAG(p0, p1)                                                                                                                                                            \
    if (mask & p0)                                                                                                                                                                 \
    flags |= p1

#define VK_SHADER_STAGE_FLAG(p0, p1) VK_FLAG(ShaderStage_##p0, vk::ShaderStageFlagBits::p1)

#define VK_IMAGE_USAGE_FLAG(p) VK_FLAG(TextureUsage_##p, vk::ImageUsageFlagBits::e##p)

#define VK_QUEUE_FLAG(p) VK_FLAG(CommandQueueCapability_##p, vk::QueueFlagBits::e##p)

#define VK_BUFFER_USAGE_FLAG(p0, p1) VK_FLAG(BufferUsage_##p0, vk::BufferUsageFlagBits::p1)

#define VK_ACCESS_FLAG_2(p) VK_FLAG(AccessMode_##p, vk::AccessFlagBits2KHR::e##p)

#define VK_PIPELINE_STAGE_FLAG_2(p0, p1) VK_FLAG(PipelineStage_##p0, vk::PipelineStageFlagBits2KHR::p1)

#define VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(p0, p1) VK_FLAG(ShaderStage_##p0, vk::PipelineStageFlagBits2KHR::p1)

#define VK_DEPENDENCY_FLAG(p) VK_FLAG(DependencyFlag_##p, vk::DependencyFlagBits::e##p)

#define VK_DESCRIPTOR_BINDING_FLAG(p) VK_FLAG(DescriptorBindingFlag_##p, vk::DescriptorBindingFlagBitsEXT::e##p)

vk::Format qhwi::vk(Format format)
{
    switch (format)
    {
        QHWI_VK_FORMAT_MAP();
    default:
        return vk::Format::eUndefined;
    };
};

vk::ComponentSwizzle qhwi::vk(ComponentSwizzle s)
{
    switch (s)
    {
        VK_CASE(ComponentSwizzle::eR);
        VK_CASE(ComponentSwizzle::eG);
        VK_CASE(ComponentSwizzle::eB);
        VK_CASE(ComponentSwizzle::eA);
        VK_CASE(ComponentSwizzle::eZero);
        VK_CASE(ComponentSwizzle::eOne);
    default:
        return vk::ComponentSwizzle(0);
    }
}

vk::BlendOp qhwi::vk(BlendOp op)
{
    switch (op)
    {
        VK_CASE(BlendOp::eAdd);
        VK_CASE(BlendOp::eMax);
        VK_CASE(BlendOp::eMin);
        VK_CASE(BlendOp::eReverseSubtract);
        VK_CASE(BlendOp::eSubtract);
    default:
        return vk::BlendOp(0);
    }
}

vk::BlendFactor qhwi::vk(BlendFactor factor)
{
    switch (factor)
    {
        VK_CASE(BlendFactor::eZero);
        VK_CASE(BlendFactor::eOne);
        VK_CASE(BlendFactor::eSrcColor);
        VK_CASE(BlendFactor::eOneMinusSrcColor);
        VK_CASE(BlendFactor::eDstColor);
        VK_CASE(BlendFactor::eOneMinusDstColor);
        VK_CASE(BlendFactor::eSrcAlpha);
        VK_CASE(BlendFactor::eOneMinusSrcAlpha);
        VK_CASE(BlendFactor::eDstAlpha);
        VK_CASE(BlendFactor::eOneMinusDstAlpha);
        VK_CASE(BlendFactor::eConstantColor);
        VK_CASE(BlendFactor::eOneMinusConstantColor);
        VK_CASE(BlendFactor::eSrcAlphaSaturate);
        VK_CASE(BlendFactor::eSrc1Color);
        VK_CASE(BlendFactor::eOneMinusSrc1Color);
        VK_CASE(BlendFactor::eSrc1Alpha);
        VK_CASE(BlendFactor::eOneMinusSrc1Alpha);
    default:
        return vk::BlendFactor(0);
    }
};

vk::LogicOp qhwi::vk(LogicOp op)
{
    switch (op)
    {
        VK_CASE(LogicOp::eClear);
        VK_CASE(LogicOp::eAnd);
        VK_CASE(LogicOp::eAndReverse);
        VK_CASE(LogicOp::eCopy);
        VK_CASE(LogicOp::eAndInverted);
        VK_CASE(LogicOp::eNoOp);
        VK_CASE(LogicOp::eXor);
        VK_CASE(LogicOp::eOr);
        VK_CASE(LogicOp::eNor);
        VK_CASE(LogicOp::eEquivalent);
        VK_CASE(LogicOp::eInvert);
        VK_CASE(LogicOp::eOrReverse);
        VK_CASE(LogicOp::eCopyInverted);
        VK_CASE(LogicOp::eOrInverted);
        VK_CASE(LogicOp::eNand);
        VK_CASE(LogicOp::eSet);
    default:
        return vk::LogicOp(0);
    }
}

vk::PolygonMode qhwi::vk(FillMode fillMode)
{
    switch (fillMode)
    {
        CASE(FillMode::eSolid, vk::PolygonMode::eFill);
        CASE(FillMode::eWireframe, vk::PolygonMode::eLine);
    default:
        return vk::PolygonMode(0);
    }
}

vk::CullModeFlags qhwi::vk(CullMode cullMode)
{
    switch (cullMode)
    {
        CASE(CullMode::eNone, vk::CullModeFlags(0));
        CASE(CullMode::eFront, vk::CullModeFlagBits::eFront);
        CASE(CullMode::eBack, vk::CullModeFlagBits::eBack);
    default:
        return vk::CullModeFlags(0);
    }
}

vk::CommandBufferLevel qhwi::vk(CommandListType commandListType)
{
    switch (commandListType)
    {
        CASE(CommandListType::ePrimary, vk::CommandBufferLevel::ePrimary);
        CASE(CommandListType::eSecondary, vk::CommandBufferLevel::eSecondary);
    default:
        return vk::CommandBufferLevel(0);
    }
}

vk::CompareOp qhwi::vk(CompareOp op)
{
    switch (op)
    {
        CASE(CompareOp::eNone, vk::CompareOp::eNever);
        VK_CASE(CompareOp::eNever);
        VK_CASE(CompareOp::eLess);
        VK_CASE(CompareOp::eEqual);
        CASE(CompareOp::eLessEqual, vk::CompareOp::eLessOrEqual);
        VK_CASE(CompareOp::eGreater);
        VK_CASE(CompareOp::eNotEqual);
        CASE(CompareOp::eGreaterEqual, vk::CompareOp::eGreaterOrEqual);
        VK_CASE(CompareOp::eAlways);
    default:
        return vk::CompareOp(0);
    }
}

vk::StencilOp qhwi::vk(StencilOp stencilOp)
{
    switch (stencilOp)
    {
        VK_CASE(StencilOp::eKeep);
        VK_CASE(StencilOp::eZero);
        VK_CASE(StencilOp::eReplace);
        VK_CASE(StencilOp::eIncrementAndClamp);
        VK_CASE(StencilOp::eDecrementAndClamp);
        VK_CASE(StencilOp::eInvert);
        VK_CASE(StencilOp::eIncrementAndWrap);
        VK_CASE(StencilOp::eDecrementAndWrap);
    default:
        return vk::StencilOp(0);
    }
};

vk::QueueFlags qhwi::vk(CommandQueueCapabilities mask)
{
    vk::QueueFlags flags;
    VK_QUEUE_FLAG(Graphics);
    VK_QUEUE_FLAG(Compute);
    VK_QUEUE_FLAG(Transfer);
    VK_FLAG(CommandQueueCapability_OpticalFlow, vk::QueueFlagBits::eOpticalFlowNV);
    VK_FLAG(CommandQueueCapability_DataGraph, (vk::QueueFlagBits)VK_QUEUE_DATA_GRAPH_BIT_ARM);
    return flags;
}

vk::AttachmentLoadOp qhwi::vk(AttachmentLoadOp attachmentLoadOp)
{
    switch (attachmentLoadOp)
    {
        VK_CASE(AttachmentLoadOp::eClear);
        VK_CASE(AttachmentLoadOp::eDontCare);
        VK_CASE(AttachmentLoadOp::eNone);
        VK_CASE(AttachmentLoadOp::eLoad);
    default:
        return vk::AttachmentLoadOp(0);
    }
}

vk::AttachmentStoreOp qhwi::vk(AttachmentStoreOp attachmentStoreOp)
{
    switch (attachmentStoreOp)
    {
        VK_CASE(AttachmentStoreOp::eDontCare);
        VK_CASE(AttachmentStoreOp::eNone);
        VK_CASE(AttachmentStoreOp::eStore);
    default:
        return vk::AttachmentStoreOp(0);
    }
}

vk::ResolveModeFlagBitsKHR qhwi::vk(ResolveMode resolveMode)
{
    switch (resolveMode)
    {
    case ResolveMode::eNone:
        return vk::ResolveModeFlagBitsKHR::eNone;
    case ResolveMode::eAverage:
        return vk::ResolveModeFlagBitsKHR::eAverage;
    case ResolveMode::eMin:
        return vk::ResolveModeFlagBitsKHR::eMin;
    case ResolveMode::eMax:
        return vk::ResolveModeFlagBitsKHR::eMax;
    default:
        return vk::ResolveModeFlagBitsKHR(0);
    }
}

vk::ImageLayout qhwi::vkImageLayout(const VkDevice* device, AccessModeMask accessMask, ImageLayout layout)
{
    switch (layout)
    {
        VK_CASE(ImageLayout::eUndefined);
    case ImageLayout::eColorResolveDest:
        return vk::ImageLayout::eColorAttachmentOptimal;
    case ImageLayout::eDepthResolveDest:
        return vk::ImageLayout::eDepthStencilAttachmentOptimal;
    case ImageLayout::eGeneral:
    case ImageLayout::eGenericRead:
    case ImageLayout::eUnorderedAccess:
        return vk::ImageLayout::eGeneral;
        VK_CASE(ImageLayout::eColorAttachmentOptimal);
        VK_CASE(ImageLayout::eDepthStencilAttachmentOptimal);
        VK_CASE(ImageLayout::eDepthStencilReadOnlyOptimal);
        VK_CASE(ImageLayout::eShaderReadOnlyOptimal);
        VK_CASE(ImageLayout::eTransferSrcOptimal);
        VK_CASE(ImageLayout::eTransferDstOptimal);
        CASE(ImageLayout::ePresent, vk::ImageLayout::ePresentSrcKHR);
    case ImageLayout::eLocalRead:
        return device->supportsDynamicRenderingLocalRead() ? vk::ImageLayout::eRenderingLocalReadKHR : vk::ImageLayout::eShaderReadOnlyOptimal;
    default:
        return vk::ImageLayout(0);
    }
}

vk::SampleCountFlagBits qhwi::vk(SampleCount sampleCount)
{
    switch (sampleCount)
    {
        VK_SAMPLE_COUNT(1);
        VK_SAMPLE_COUNT(2);
        VK_SAMPLE_COUNT(4);
        VK_SAMPLE_COUNT(8);
        VK_SAMPLE_COUNT(16);
        VK_SAMPLE_COUNT(32);
        VK_SAMPLE_COUNT(64);
    default:
        return vk::SampleCountFlagBits(0);
    }
}

vk::ShaderStageFlagBits qhwi::vk(ShaderStage shaderStage)
{
    switch (shaderStage)
    {
        CASE(ShaderStage_Vertex, vk::ShaderStageFlagBits::eVertex);
        CASE(ShaderStage_Pixel, vk::ShaderStageFlagBits::eFragment);
        CASE(ShaderStage_Domain, vk::ShaderStageFlagBits::eTessellationEvaluation);
        CASE(ShaderStage_Hull, vk::ShaderStageFlagBits::eTessellationControl);
        CASE(ShaderStage_Geometry, vk::ShaderStageFlagBits::eGeometry);
        CASE(ShaderStage_Compute, vk::ShaderStageFlagBits::eCompute);
        CASE(ShaderStage_Amplification, vk::ShaderStageFlagBits::eTaskEXT);
        CASE(ShaderStage_Mesh, vk::ShaderStageFlagBits::eMeshEXT);
        CASE(ShaderStage_RayGeneration, vk::ShaderStageFlagBits::eRaygenKHR);
        CASE(ShaderStage_AnyHit, vk::ShaderStageFlagBits::eAnyHitKHR);
        CASE(ShaderStage_ClosestHit, vk::ShaderStageFlagBits::eClosestHitKHR);
        CASE(ShaderStage_Miss, vk::ShaderStageFlagBits::eMissKHR);
        CASE(ShaderStage_Intersection, vk::ShaderStageFlagBits::eIntersectionKHR);
        CASE(ShaderStage_Callable, vk::ShaderStageFlagBits::eCallableKHR);
    default:
        return vk::ShaderStageFlagBits(0);
    }
}

vk::RayTracingShaderGroupTypeKHR qhwi::vk(RayTracingShaderGroupType type)
{
    switch (type)
    {
        CASE(RayTracingShaderGroupType::eGeneral, vk::RayTracingShaderGroupTypeKHR::eGeneral);
        CASE(RayTracingShaderGroupType::eTriangleHit, vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
        CASE(RayTracingShaderGroupType::eProceduralHit, vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup);
    default:
        return vk::RayTracingShaderGroupTypeKHR::eGeneral;
    }
}

vk::ShaderStageFlags qhwi::vkShaderStageFlags(ShaderStageMask mask)
{
    vk::ShaderStageFlags flags;
    VK_SHADER_STAGE_FLAG(Vertex, eVertex);
    VK_SHADER_STAGE_FLAG(Pixel, eFragment);
    VK_SHADER_STAGE_FLAG(Domain, eTessellationEvaluation);
    VK_SHADER_STAGE_FLAG(Hull, eTessellationControl);
    VK_SHADER_STAGE_FLAG(Geometry, eGeometry);
    VK_SHADER_STAGE_FLAG(Compute, eCompute);
    VK_SHADER_STAGE_FLAG(Amplification, eTaskEXT);
    VK_SHADER_STAGE_FLAG(Mesh, eMeshEXT);
    VK_SHADER_STAGE_FLAG(RayGeneration, eRaygenKHR);
    VK_SHADER_STAGE_FLAG(AnyHit, eAnyHitKHR);
    VK_SHADER_STAGE_FLAG(ClosestHit, eClosestHitKHR);
    VK_SHADER_STAGE_FLAG(Intersection, eIntersectionKHR);
    VK_SHADER_STAGE_FLAG(Callable, eCallableKHR);
    VK_SHADER_STAGE_FLAG(Miss, eMissKHR);
    return flags;
}

vk::PrimitiveTopology qhwi::vk(PrimitiveTopology topology)
{
    switch (topology)
    {
        VK_CASE(PrimitiveTopology::eLineList);
        VK_CASE(PrimitiveTopology::eLineStrip);
        VK_CASE(PrimitiveTopology::ePointList);
        VK_CASE(PrimitiveTopology::eTriangleList);
        VK_CASE(PrimitiveTopology::eTriangleStrip);
    default:
        return vk::PrimitiveTopology(0);
    }
}

vk::VertexInputRate qhwi::vk(VertexInputRate vertexInputRate)
{
    switch (vertexInputRate)
    {
        VK_CASE(VertexInputRate::eInstance);
        VK_CASE(VertexInputRate::eVertex);
    default:
        return vk::VertexInputRate(0);
    }
}

vk::IndexType qhwi::vk(IndexFormat format)
{
    switch (format)
    {
        CASE(IndexFormat::eUint16, vk::IndexType::eUint16);
        CASE(IndexFormat::eUint32, vk::IndexType::eUint32);
    default:
        return vk::IndexType(0);
    }
}

vk::DescriptorType qhwi::vk(DescriptorType type)
{
    switch (type)
    {
        CASE(DescriptorType::eConstantBuffer, vk::DescriptorType::eUniformBuffer);
        CASE(DescriptorType::eDynamicConstantBuffer, vk::DescriptorType::eUniformBufferDynamic);
        CASE(DescriptorType::eTexture, vk::DescriptorType::eSampledImage);
        CASE(DescriptorType::eSampler, vk::DescriptorType::eSampler);
        CASE(DescriptorType::eStructuredBuffer, vk::DescriptorType::eStorageBuffer);
        CASE(DescriptorType::eByteAddressBuffer, vk::DescriptorType::eStorageBuffer);
        CASE(DescriptorType::eTypedBuffer, vk::DescriptorType::eUniformTexelBuffer);
        CASE(DescriptorType::eRWStructuredBuffer, vk::DescriptorType::eStorageBuffer);
        CASE(DescriptorType::eRWByteAddressBuffer, vk::DescriptorType::eStorageBuffer);
        CASE(DescriptorType::eRWTypedBuffer, vk::DescriptorType::eStorageTexelBuffer);
        CASE(DescriptorType::eRWTexture, vk::DescriptorType::eStorageImage);
        CASE(DescriptorType::eInputAttachment, vk::DescriptorType::eInputAttachment);
        CASE(DescriptorType::eAccelerationStructure, vk::DescriptorType::eAccelerationStructureKHR);
        CASE(DescriptorType::eBlockMatchImageQCOM, vk::DescriptorType::eBlockMatchImageQCOM);
        CASE(DescriptorType::eSampleWeightImageQCOM, vk::DescriptorType::eSampleWeightImageQCOM);
    default:
        return vk::DescriptorType(0);
    }
}

vk::BufferUsageFlags qhwi::vkBufferUsageFlags(BufferUsageMask mask, HeapType heapType)
{
    vk::BufferUsageFlags flags;
    VK_BUFFER_USAGE_FLAG(VertexBuffer, eVertexBuffer);
    VK_BUFFER_USAGE_FLAG(IndexBuffer, eIndexBuffer);
    VK_BUFFER_USAGE_FLAG(ConstantBuffer, eUniformBuffer);
    VK_BUFFER_USAGE_FLAG(StructuredBuffer, eStorageBuffer);
    VK_BUFFER_USAGE_FLAG(ByteAddressBuffer, eStorageBuffer);
    VK_BUFFER_USAGE_FLAG(TypedBuffer, eUniformTexelBuffer);
    VK_BUFFER_USAGE_FLAG(RWStructuredBuffer, eStorageBuffer);
    VK_BUFFER_USAGE_FLAG(RWByteAddressBuffer, eStorageBuffer);
    VK_BUFFER_USAGE_FLAG(RWTypedBuffer, eStorageTexelBuffer);
    VK_BUFFER_USAGE_FLAG(IndirectBuffer, eIndirectBuffer);
    VK_BUFFER_USAGE_FLAG(ShaderBindingTable, eShaderBindingTableKHR);

    if (mask & BufferUsage_AccelerationStructure)
    {
        flags |= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR;
    }

    if (mask & BufferUsage_AccelerationStructureBuildSource)
    {
        flags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
    }

    if (mask & BufferUsage_TileMemoryQCOM)
    {
        flags |= vk::BufferUsageFlagBits(VK_BUFFER_USAGE_TILE_MEMORY_BIT_QCOM);
    }
    else
    {
        if (heapType == HeapType::eDownload)
        {
            flags |= vk::BufferUsageFlagBits::eTransferDst;
        }
        else
        {
            flags |= vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
        }
    }

    flags |= vk::BufferUsageFlagBits::eShaderDeviceAddressEXT;

    return flags;
}

vk::PipelineBindPoint qhwi::vk(PipelineBindPoint pipelineBindPoint)
{
    switch (pipelineBindPoint)
    {
        VK_CASE(PipelineBindPoint::eCompute);
        VK_CASE(PipelineBindPoint::eGraphics);
        CASE(PipelineBindPoint::eRayTracing, vk::PipelineBindPoint::eRayTracingKHR);
    default:
        return vk::PipelineBindPoint(0);
    }
}

vk::Filter qhwi::vk(Filter filter)
{
    switch (filter)
    {
        VK_CASE(Filter::eLinear);
        VK_CASE(Filter::eNearest);
    default:
        return vk::Filter(0);
    }
}

vk::SamplerAddressMode qhwi::vk(SamplerAddressMode mode)
{
    switch (mode)
    {
        VK_CASE(SamplerAddressMode::eClampToEdge);
        VK_CASE(SamplerAddressMode::eClampToBorder);
        VK_CASE(SamplerAddressMode::eMirrorClampToEdge);
        VK_CASE(SamplerAddressMode::eMirroredRepeat);
        VK_CASE(SamplerAddressMode::eRepeat);
    default:
        return vk::SamplerAddressMode(0);
    }
}

vk::SamplerReductionMode qhwi::vk(SamplerReductionMode mode)
{
    switch (mode)
    {
        VK_CASE(SamplerReductionMode::eWeightedAverage);
        VK_CASE(SamplerReductionMode::eMin);
        VK_CASE(SamplerReductionMode::eMax);
    default:
        return vk::SamplerReductionMode(0);
    }
}

vk::ImageViewType qhwi::vk(TextureType type)
{
    switch (type)
    {
        CASE(TextureType::e1D, vk::ImageViewType::e1D);
        CASE(TextureType::e2D, vk::ImageViewType::e2D);
        CASE(TextureType::e3D, vk::ImageViewType::e3D);
        CASE(TextureType::e1DArray, vk::ImageViewType::e1DArray);
        CASE(TextureType::e2DArray, vk::ImageViewType::e2DArray);
        CASE(TextureType::eCubeArray, vk::ImageViewType::eCubeArray);
        CASE(TextureType::eCube, vk::ImageViewType::eCube);
    default:
        return vk::ImageViewType(0);
    }
}

vk::SamplerCreateFlags qhwi::vkSamplerCreateFlags(SamplerCreateFlags flags)
{
    vk::SamplerCreateFlags vkFlags;
    if (flags & SamplerCreateFlag_ImageProcessingQCOM)
    {
        vkFlags |= vk::SamplerCreateFlagBits::eImageProcessingQCOM;
    }
    return vkFlags;
}

vk::SamplerMipmapMode qhwi::vkSamplerMipmapMode(Filter filter)
{
    switch (filter)
    {
        CASE(Filter::eLinear, vk::SamplerMipmapMode::eLinear);
        CASE(Filter::eNearest, vk::SamplerMipmapMode::eNearest);
    default:
        return vk::SamplerMipmapMode(0);
    }
}

vk::ImageUsageFlags qhwi::vkImageUsageFlags(TextureUsageMask mask)
{
    vk::ImageUsageFlags flags;
    VK_IMAGE_USAGE_FLAG(ColorAttachment);
    VK_IMAGE_USAGE_FLAG(DepthStencilAttachment);
    VK_IMAGE_USAGE_FLAG(Sampled);
    VK_IMAGE_USAGE_FLAG(TransientAttachment);
    VK_IMAGE_USAGE_FLAG(InputAttachment);
    VK_IMAGE_USAGE_FLAG(Storage);
    VK_IMAGE_USAGE_FLAG(SampleBlockMatchQCOM);
    VK_IMAGE_USAGE_FLAG(SampleWeightQCOM);
    if (mask & TextureUsage_TileMemoryQCOM)
    {
        flags |= vk::ImageUsageFlagBits(VK_IMAGE_USAGE_TILE_MEMORY_BIT_QCOM);
    }
    return flags;
}

vk::AccessFlags2KHR qhwi::vkAccessFlags2(AccessModeMask mask)
{
    vk::AccessFlags2KHR flags;
    VK_FLAG(AccessMode_IndirectCommandBufferRead, vk::AccessFlagBits2KHR::eIndirectCommandRead);
    VK_FLAG(AccessMode_IndexBufferRead, vk::AccessFlagBits2KHR::eIndexRead);
    VK_FLAG(AccessMode_VertexAttributeBufferRead, vk::AccessFlagBits2KHR::eVertexAttributeRead);
    VK_FLAG(AccessMode_ConstantBufferRead, vk::AccessFlagBits2KHR::eUniformRead);
    VK_FLAG(AccessMode_ColorResolveDest, vk::AccessFlagBits2KHR::eColorAttachmentWrite);
    VK_FLAG(AccessMode_DepthResolveDest, vk::AccessFlagBits2KHR::eDepthStencilAttachmentWrite | vk::AccessFlagBits2KHR::eColorAttachmentWrite);
    VK_ACCESS_FLAG_2(InputAttachmentRead);
    VK_ACCESS_FLAG_2(ColorAttachmentRead);
    VK_ACCESS_FLAG_2(ColorAttachmentWrite);
    VK_ACCESS_FLAG_2(DepthStencilAttachmentRead);
    VK_ACCESS_FLAG_2(DepthStencilAttachmentWrite);
    VK_ACCESS_FLAG_2(TransferRead);
    VK_ACCESS_FLAG_2(TransferWrite);
    VK_ACCESS_FLAG_2(HostRead);
    VK_ACCESS_FLAG_2(HostWrite);
    VK_ACCESS_FLAG_2(MemoryRead);
    VK_ACCESS_FLAG_2(MemoryWrite);
    VK_ACCESS_FLAG_2(ShaderSampledRead);
    VK_ACCESS_FLAG_2(ShaderStorageRead);
    VK_ACCESS_FLAG_2(ShaderStorageWrite);
    VK_FLAG(AccessMode_OpticalFlowRead, vk::AccessFlagBits2KHR::eOpticalFlowReadNV);
    VK_FLAG(AccessMode_OpticalFlowWrite, vk::AccessFlagBits2KHR::eOpticalFlowWriteNV);
    VK_FLAG(AccessMode_TileAttachmentReadQcom, vk::AccessFlagBits2(VK_ACCESS_2_SHADER_TILE_ATTACHMENT_READ_BIT_QCOM));
    VK_FLAG(AccessMode_TileAttachmentWriteQcom, vk::AccessFlagBits2(VK_ACCESS_2_SHADER_TILE_ATTACHMENT_WRITE_BIT_QCOM));
    return flags;
}

vk::PipelineStageFlags2KHR qhwi::vkPipelineStageFlags2(PipelineStageMask mask)
{
    vk::PipelineStageFlags2KHR flags;
    VK_PIPELINE_STAGE_FLAG_2(None, eNone);
    VK_PIPELINE_STAGE_FLAG_2(TopOfPipe, eTopOfPipe);
    VK_PIPELINE_STAGE_FLAG_2(DrawIndirect, eDrawIndirect);
    VK_PIPELINE_STAGE_FLAG_2(VertexInput, eVertexAttributeInput);
    VK_PIPELINE_STAGE_FLAG_2(VertexShader, eVertexShader);
    VK_PIPELINE_STAGE_FLAG_2(HullShader, eTessellationControlShader);
    VK_PIPELINE_STAGE_FLAG_2(DomainShader, eTessellationEvaluationShader);
    VK_PIPELINE_STAGE_FLAG_2(GeometryShader, eGeometryShader);
    VK_PIPELINE_STAGE_FLAG_2(PixelShader, eFragmentShader);
    VK_PIPELINE_STAGE_FLAG_2(EarlyPixelTests, eEarlyFragmentTests);
    VK_PIPELINE_STAGE_FLAG_2(LatePixelTests, eLateFragmentTests);
    VK_PIPELINE_STAGE_FLAG_2(ColorAttachmentOutput, eColorAttachmentOutput);
    VK_PIPELINE_STAGE_FLAG_2(ComputeShader, eComputeShader);
    VK_PIPELINE_STAGE_FLAG_2(Transfer, eTransfer);
    VK_PIPELINE_STAGE_FLAG_2(BottomOfPipe, eBottomOfPipe);
    VK_PIPELINE_STAGE_FLAG_2(Host, eHost);
    VK_PIPELINE_STAGE_FLAG_2(AllGraphics, eAllGraphics);
    VK_PIPELINE_STAGE_FLAG_2(AllCommands, eAllCommands);
    VK_PIPELINE_STAGE_FLAG_2(IndexInput, eIndexInput);
    VK_PIPELINE_STAGE_FLAG_2(AmplificationShader, eTaskShaderEXT);
    VK_PIPELINE_STAGE_FLAG_2(MeshShader, eMeshShaderEXT);
    VK_PIPELINE_STAGE_FLAG_2(AccelerationStructureBuild, eAccelerationStructureBuildKHR);
    VK_PIPELINE_STAGE_FLAG_2(OpticalFlow, eOpticalFlowNV);
    VK_PIPELINE_STAGE_FLAG_2(RayTracingShader, eRayTracingShaderKHR);
    VK_PIPELINE_STAGE_FLAG_2(Clear, eClear);
    return flags;
}

vk::PipelineStageFlags2KHR qhwi::vkPipelineStageFlags2FromShaderStageMask(ShaderStageMask mask)
{
    vk::PipelineStageFlags2KHR flags;
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Vertex, eVertexShader);
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Hull, eTessellationControlShader);
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Domain, eTessellationEvaluationShader);
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Geometry, eGeometryShader);
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Pixel, eFragmentShader);
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Compute, eComputeShader);
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Amplification, eTaskShaderEXT);
    VK_PIPELINE_STAGE_FLAG_2_FROM_SHADER_STAGE(Mesh, eMeshShaderEXT);
    return flags;
}

vk::DependencyFlags qhwi::vkDependencyFlags(DependencyFlags mask)
{
    vk::DependencyFlags flags;
    VK_DEPENDENCY_FLAG(ByRegion);
    VK_DEPENDENCY_FLAG(DeviceGroup);
    VK_DEPENDENCY_FLAG(ViewLocal);
    return flags;
}

vk::DescriptorBindingFlagsEXT qhwi::vkDescriptorBindingFlags(DescriptorBindingFlags mask)
{
    vk::DescriptorBindingFlagsEXT flags;
    VK_DESCRIPTOR_BINDING_FLAG(UpdateAfterBind);
    VK_DESCRIPTOR_BINDING_FLAG(PartiallyBound);
    VK_DESCRIPTOR_BINDING_FLAG(VariableDescriptorCount);
    return flags;
}

vk::BuildAccelerationStructureModeKHR qhwi::vk(AccelerationStructureBuildMode mode)
{
    switch (mode)
    {
    case AccelerationStructureBuildMode::eBuild:
    default:
        return vk::BuildAccelerationStructureModeKHR::eBuild;
    case AccelerationStructureBuildMode::eUpdate:
        return vk::BuildAccelerationStructureModeKHR::eUpdate;
    }
}

vk::BuildAccelerationStructureFlagsKHR qhwi::vkBuildAccelerationStructureFlags(AccelerationStructureBuildFlags flags)
{
    vk::BuildAccelerationStructureFlagsKHR r = (vk::BuildAccelerationStructureFlagsKHR)0;
    if (flags & AccelerationStructureBuildFlag_AllowUpdate)
    {
        r |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
    }
    if (flags & AccelerationStructureBuildFlag_AllowCompaction)
    {
        r |= vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
    }
    if (flags & AccelerationStructureBuildFlag_PreferFastTrace)
    {
        r |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    }
    if (flags & AccelerationStructureBuildFlag_PreferFastBuild)
    {
        r |= vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild;
    }
    if (flags & AccelerationStructureBuildFlag_MinimizeMemory)
    {
        r |= vk::BuildAccelerationStructureFlagBitsKHR::eLowMemory;
    }
    return r;
}