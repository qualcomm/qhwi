//
//
//  qhwi_d3d12_enums.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_D3D12_ENUMS_H__
#define __QHWI_D3D12_ENUMS_H__

#include <qhwi/qhwi_enums.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
#    include <dstorage.h>
#endif

namespace qhwi
{
DXGI_FORMAT d3d(Format format);
DXGI_FORMAT d3dSrvFormat(DXGI_FORMAT format);
D3D12_SHADER_COMPONENT_MAPPING d3d(ComponentSwizzle s);
D3D12_BLEND_OP d3d(BlendOp op);
D3D12_BLEND d3d(BlendFactor factor);
D3D12_LOGIC_OP d3d(LogicOp op);
D3D12_COLOR_WRITE_ENABLE d3d(ColorChannel channel);
D3D12_SRV_DIMENSION d3dSrvDimension(TextureType textureType, SampleCount sampleCount);
D3D12_TEXTURE_ADDRESS_MODE d3d(SamplerAddressMode samplerAddressMode);
D3D12_FILL_MODE d3d(FillMode fillMode);
D3D12_CULL_MODE d3d(CullMode cullMode);
D3D12_COMPARISON_FUNC d3d(CompareOp op);
D3D12_STENCIL_OP d3d(StencilOp op);
D3D12_PRIMITIVE_TOPOLOGY d3d(PrimitiveTopology topology);
D3D12_INPUT_CLASSIFICATION d3d(VertexInputRate vertexInputRate);
D3D12_COMMAND_LIST_TYPE d3d(CommandListType commandListType);
D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE d3d(AttachmentLoadOp op);
D3D12_RENDER_PASS_ENDING_ACCESS_TYPE d3d(AttachmentStoreOp op);
D3D12_COMMAND_LIST_TYPE d3d(CommandQueueCapabilities caps);
DXGI_FORMAT d3d(IndexFormat format);
D3D12_HEAP_TYPE d3d(HeapType heapType);
D3D12_HEAP_FLAGS d3dHeapFlags(HeapFlags heapFlags);
D3D12_FILTER d3d(Filter minFilter, Filter magFilter, Filter mipFilter, CompareOp c);
D3D12_BARRIER_LAYOUT d3dBarrierLayout(AccessModeMask accessModeMask, ImageLayout layout);
D3D12_RESOLVE_MODE d3d(ResolveMode resolveMode);
D3D12_BARRIER_ACCESS d3dBarrierAccess(PipelineStageMask pipelineStages, AccessModeMask accessModes);
D3D12_BARRIER_SYNC d3dBarrierSync(PipelineStageMask pipelineStages, AccessModeMask accessModes);
D3D12_PRIMITIVE_TOPOLOGY_TYPE d3dPrimitiveTopologyType(PrimitiveTopology primitiveTopology);
D3D12_SHADER_VISIBILITY d3dShaderVisibility(ShaderStageMask shaderStages);
D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS d3dRaytracingAccelerationStructureBuildFlags(AccelerationStructureBuildMode mode, AccelerationStructureBuildFlags flags);

DXGI_USAGE dxgiUsage(TextureUsageMask mask);

Format qhwi(DXGI_FORMAT format);

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
namespace compressedstorage
{
DSTORAGE_REQUEST_SOURCE_TYPE dstorageRequestSourceType(StorageRequestSourceType sourceType);
DSTORAGE_REQUEST_DESTINATION_TYPE dstorageRequestDestinationType(StorageRequestDestinationType destinationType);
DSTORAGE_PRIORITY dstoragePriority(StoragePriority priority);
DSTORAGE_COMPRESSION_FORMAT dstorageCompressionFormat(CompressionFormat format);
} // namespace compressedstorage
#endif // QHWI_ENABLE_COMPRESSED_STORAGE
} // namespace qhwi
#endif
