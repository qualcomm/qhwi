//
//
//  qhwi_vk_enums.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_VK_ENUMS_H__
#define __QHWI_VK_ENUMS_H__

#include <qhwi/qhwi_enums.h>
#include <qhwi/qhwi_vk_config.h>

namespace qhwi
{
class VkDevice;

vk::Format vk(Format format);
vk::ComponentSwizzle vk(ComponentSwizzle s);
vk::BlendOp vk(BlendOp op);
vk::BlendFactor vk(BlendFactor factor);
vk::LogicOp vk(LogicOp op);
vk::PolygonMode vk(FillMode fillMode);
vk::CullModeFlags vk(CullMode cullMode);
vk::CompareOp vk(CompareOp op);
vk::StencilOp vk(StencilOp op);
vk::PrimitiveTopology vk(PrimitiveTopology topology);
vk::VertexInputRate vk(VertexInputRate vertexInputRate);
vk::CommandBufferLevel vk(CommandListType commandListType);
vk::AttachmentLoadOp vk(AttachmentLoadOp attachmentLoadOp);
vk::AttachmentStoreOp vk(AttachmentStoreOp attachmentStoreOp);
vk::ResolveModeFlagBitsKHR vk(ResolveMode resolveMode);
vk::ImageLayout vkImageLayout(const VkDevice* device, AccessModeMask accessMask, ImageLayout layout);
vk::SampleCountFlagBits vk(SampleCount sampleCount);
vk::QueueFlags vk(CommandQueueCapabilities caps);
vk::ShaderStageFlagBits vk(ShaderStage shaderStage);
vk::IndexType vk(IndexFormat format);
vk::DescriptorType vk(DescriptorType type);
vk::PipelineBindPoint vk(PipelineBindPoint bindPoint);
vk::Filter vk(Filter filter);
vk::SamplerAddressMode vk(SamplerAddressMode mode);
vk::SamplerReductionModeEXT vk(SamplerReductionMode mode);
vk::ImageViewType vk(TextureType type);
vk::RayTracingShaderGroupTypeKHR vk(RayTracingShaderGroupType type);
vk::SamplerCreateFlags vkSamplerCreateFlags(SamplerCreateFlags flags);
vk::BuildAccelerationStructureModeKHR vk(AccelerationStructureBuildMode mode);
vk::BuildAccelerationStructureFlagsKHR vkBuildAccelerationStructureFlags(AccelerationStructureBuildFlags flags);

vk::SamplerMipmapMode vkSamplerMipmapMode(Filter filter);
vk::ShaderStageFlags vkShaderStageFlags(ShaderStageMask shaderStages);
vk::BufferUsageFlags vkBufferUsageFlags(BufferUsageMask mask, HeapType heapType);
vk::ImageUsageFlags vkImageUsageFlags(TextureUsageMask mask);

vk::AccessFlags2KHR vkAccessFlags2(AccessModeMask mask);
vk::PipelineStageFlags2KHR vkPipelineStageFlags2(PipelineStageMask mask);
vk::PipelineStageFlags2KHR vkPipelineStageFlags2FromShaderStageMask(ShaderStageMask shaderStages);

vk::DependencyFlags vkDependencyFlags(DependencyFlags flags);
vk::DescriptorBindingFlagsEXT vkDescriptorBindingFlags(DescriptorBindingFlags flags);

Format qhwi(vk::Format format);

bool supportsDepth(vk::Format format);
bool supportsStencil(vk::Format format);
} // namespace qhwi
#endif