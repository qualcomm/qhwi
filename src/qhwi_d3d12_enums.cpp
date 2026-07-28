//
//
//  qhwi_d3d12_enums.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <qhwi/qhwi_d3d12_enums.h>
#include "qhwi_d3d12_enums_internal.h"
#include <qhwi/qhwi_log_util.h>
#include <cstdint>
#include <utility>

#define CASE(p0, p1)                                                                                                                                                               \
    case p0:                                                                                                                                                                       \
        return p1

#define D3D12_BARRIER_LAYOUT_CASE(s, t)                                                                                                                                            \
    case ImageLayout::e##s:                                                                                                                                                        \
        return D3D12_BARRIER_LAYOUT_##t
#define D3D12_BARRIER_SYNC_CASE(s, t)                                                                                                                                              \
    if (pipelineStages & PipelineStage_##s)                                                                                                                                        \
    sync |= D3D12_BARRIER_SYNC_##t
#define D3D12_BARRIER_ACCESS_CASE(s, t)                                                                                                                                            \
    if (accessModes & AccessMode_##s)                                                                                                                                              \
    access |= D3D12_BARRIER_ACCESS_##t
#define DXGI_USAGE_FLAG(s, t)                                                                                                                                                      \
    if (mask & TextureUsage_##s)                                                                                                                                                   \
    usage |= DXGI_USAGE_##t

DXGI_FORMAT qhwi::d3d(Format format)
{
    switch (format)
    {
        QHWI_DXGI_FORMAT_MAP();
    default:
        return DXGI_FORMAT_UNKNOWN;
    };
};

D3D12_SHADER_COMPONENT_MAPPING qhwi::d3d(ComponentSwizzle s)
{
    switch (s)
    {
    case ComponentSwizzle::eR:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
    case ComponentSwizzle::eG:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
    case ComponentSwizzle::eB:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
    case ComponentSwizzle::eA:
        return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
    case ComponentSwizzle::eZero:
        return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
    case ComponentSwizzle::eOne:
        return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
    default:
        QRE_UNREACHABLE();
    }
}

DXGI_FORMAT qhwi::d3dSrvFormat(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_D16_UNORM:
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:
        return format;
    }
}

DXGI_FORMAT qhwi::d3d(IndexFormat format)
{
    switch (format)
    {
        CASE(IndexFormat::eUint16, DXGI_FORMAT_R16_UINT);
        CASE(IndexFormat::eUint32, DXGI_FORMAT_R32_UINT);
    default:
        return DXGI_FORMAT(0);
    }
}

D3D12_HEAP_TYPE qhwi::d3d(HeapType heapType)
{
    switch (heapType)
    {
        CASE(HeapType::eDefault, D3D12_HEAP_TYPE_DEFAULT);
        CASE(HeapType::eUpload, D3D12_HEAP_TYPE_UPLOAD);
        CASE(HeapType::eDownload, D3D12_HEAP_TYPE_READBACK);
    default:
        return D3D12_HEAP_TYPE(0);
    }
}

D3D12_HEAP_FLAGS qhwi::d3dHeapFlags(HeapFlags heapFlags)
{
    D3D12_HEAP_FLAGS d3dHeapFlags = D3D12_HEAP_FLAG_NONE;
    if (heapFlags & HeapFlag_Shared)
    {
        d3dHeapFlags |= D3D12_HEAP_FLAG_SHARED;
    }
    return d3dHeapFlags;
}

D3D12_BLEND_OP qhwi::d3d(BlendOp op)
{
    switch (op)
    {
        CASE(BlendOp::eAdd, D3D12_BLEND_OP_ADD);
        CASE(BlendOp::eSubtract, D3D12_BLEND_OP_SUBTRACT);
        CASE(BlendOp::eReverseSubtract, D3D12_BLEND_OP_REV_SUBTRACT);
        CASE(BlendOp::eMin, D3D12_BLEND_OP_MIN);
        CASE(BlendOp::eMax, D3D12_BLEND_OP_MAX);
    default:
        return D3D12_BLEND_OP(0);
    }
};

D3D12_BLEND qhwi::d3d(BlendFactor factor)
{
    switch (factor)
    {
        CASE(BlendFactor::eZero, D3D12_BLEND_ZERO);
        CASE(BlendFactor::eOne, D3D12_BLEND_ONE);
        CASE(BlendFactor::eSrcColor, D3D12_BLEND_SRC_COLOR);
        CASE(BlendFactor::eOneMinusSrcColor, D3D12_BLEND_INV_SRC_COLOR);
        CASE(BlendFactor::eDstColor, D3D12_BLEND_DEST_COLOR);
        CASE(BlendFactor::eOneMinusDstColor, D3D12_BLEND_INV_DEST_COLOR);
        CASE(BlendFactor::eSrcAlpha, D3D12_BLEND_SRC_ALPHA);
        CASE(BlendFactor::eOneMinusSrcAlpha, D3D12_BLEND_INV_SRC_ALPHA);
        CASE(BlendFactor::eDstAlpha, D3D12_BLEND_DEST_ALPHA);
        CASE(BlendFactor::eOneMinusDstAlpha, D3D12_BLEND_INV_DEST_ALPHA);
        CASE(BlendFactor::eConstantColor, D3D12_BLEND_BLEND_FACTOR);
        CASE(BlendFactor::eOneMinusConstantColor, D3D12_BLEND_INV_BLEND_FACTOR);
        CASE(BlendFactor::eSrcAlphaSaturate, D3D12_BLEND_SRC_ALPHA_SAT);
        CASE(BlendFactor::eSrc1Color, D3D12_BLEND_SRC1_COLOR);
        CASE(BlendFactor::eOneMinusSrc1Color, D3D12_BLEND_INV_SRC1_COLOR);
        CASE(BlendFactor::eSrc1Alpha, D3D12_BLEND_SRC1_ALPHA);
        CASE(BlendFactor::eOneMinusSrc1Alpha, D3D12_BLEND_INV_SRC1_ALPHA);
    default:
        return D3D12_BLEND(0);
    }
};

D3D12_LOGIC_OP qhwi::d3d(LogicOp op)
{
    switch (op)
    {
        CASE(LogicOp::eClear, D3D12_LOGIC_OP_CLEAR);
        CASE(LogicOp::eAnd, D3D12_LOGIC_OP_AND);
        CASE(LogicOp::eAndReverse, D3D12_LOGIC_OP_AND_REVERSE);
        CASE(LogicOp::eCopy, D3D12_LOGIC_OP_COPY);
        CASE(LogicOp::eAndInverted, D3D12_LOGIC_OP_AND_INVERTED);
        CASE(LogicOp::eNoOp, D3D12_LOGIC_OP_NOOP);
        CASE(LogicOp::eXor, D3D12_LOGIC_OP_XOR);
        CASE(LogicOp::eOr, D3D12_LOGIC_OP_OR);
        CASE(LogicOp::eNor, D3D12_LOGIC_OP_NOR);
        CASE(LogicOp::eEquivalent, D3D12_LOGIC_OP_EQUIV);
        CASE(LogicOp::eInvert, D3D12_LOGIC_OP_INVERT);
        CASE(LogicOp::eOrReverse, D3D12_LOGIC_OP_OR_REVERSE);
        CASE(LogicOp::eCopyInverted, D3D12_LOGIC_OP_COPY_INVERTED);
        CASE(LogicOp::eOrInverted, D3D12_LOGIC_OP_OR_INVERTED);
        CASE(LogicOp::eNand, D3D12_LOGIC_OP_NAND);
        CASE(LogicOp::eSet, D3D12_LOGIC_OP_SET);
    default:
        return D3D12_LOGIC_OP(0);
    }
}

D3D12_COLOR_WRITE_ENABLE qhwi::d3d(ColorChannel channel)
{
    switch (channel)
    {
        CASE(ColorChannel::eR, D3D12_COLOR_WRITE_ENABLE_RED);
        CASE(ColorChannel::eG, D3D12_COLOR_WRITE_ENABLE_GREEN);
        CASE(ColorChannel::eB, D3D12_COLOR_WRITE_ENABLE_BLUE);
        CASE(ColorChannel::eA, D3D12_COLOR_WRITE_ENABLE_ALPHA);
        CASE(ColorChannel::eAll, D3D12_COLOR_WRITE_ENABLE_ALL);
    default:
        return D3D12_COLOR_WRITE_ENABLE(0);
    }
};

D3D12_SRV_DIMENSION qhwi::d3dSrvDimension(TextureType textureType, SampleCount sampleCount)
{
    switch (textureType)
    {
        CASE(TextureType::e1D, D3D12_SRV_DIMENSION_TEXTURE1D);
    case TextureType::e2D:
        if (sampleCount == SampleCount::e1)
        {
            return D3D12_SRV_DIMENSION_TEXTURE2D;
        }
        else
        {
            return D3D12_SRV_DIMENSION_TEXTURE2DMS;
        }
        CASE(TextureType::e3D, D3D12_SRV_DIMENSION_TEXTURE3D);
        CASE(TextureType::eCube, D3D12_SRV_DIMENSION_TEXTURECUBE);
        CASE(TextureType::e1DArray, D3D12_SRV_DIMENSION_TEXTURE1DARRAY);
    case TextureType::e2DArray:
        if (sampleCount == SampleCount::e1)
        {
            return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        }
        else
        {
            return D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
        }
        CASE(TextureType::eCubeArray, D3D12_SRV_DIMENSION_TEXTURECUBEARRAY);
    default:
        return D3D12_SRV_DIMENSION(0);
    }
};

D3D12_TEXTURE_ADDRESS_MODE qhwi::d3d(SamplerAddressMode addressMode)
{
    switch (addressMode)
    {
        CASE(SamplerAddressMode::eRepeat, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
        CASE(SamplerAddressMode::eMirroredRepeat, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
        CASE(SamplerAddressMode::eClampToEdge, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        CASE(SamplerAddressMode::eClampToBorder, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
        CASE(SamplerAddressMode::eMirrorClampToEdge, D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE);
    default:
        return D3D12_TEXTURE_ADDRESS_MODE(0);
    };
};

D3D12_FILL_MODE qhwi::d3d(FillMode fillMode)
{
    switch (fillMode)
    {
        CASE(FillMode::eSolid, D3D12_FILL_MODE_SOLID);
        CASE(FillMode::eWireframe, D3D12_FILL_MODE_WIREFRAME);
    default:
        return D3D12_FILL_MODE(0);
    }
};

D3D12_CULL_MODE qhwi::d3d(CullMode cullMode)
{
    switch (cullMode)
    {
        CASE(CullMode::eNone, D3D12_CULL_MODE_NONE);
        CASE(CullMode::eFront, D3D12_CULL_MODE_FRONT);
        CASE(CullMode::eBack, D3D12_CULL_MODE_BACK);
    default:
        return D3D12_CULL_MODE(0);
    }
};

D3D12_COMPARISON_FUNC qhwi::d3d(CompareOp compareOp)
{
    switch (compareOp)
    {
        CASE(CompareOp::eNone, D3D12_COMPARISON_FUNC_NONE);
        CASE(CompareOp::eNever, D3D12_COMPARISON_FUNC_NEVER);
        CASE(CompareOp::eLess, D3D12_COMPARISON_FUNC_LESS);
        CASE(CompareOp::eEqual, D3D12_COMPARISON_FUNC_EQUAL);
        CASE(CompareOp::eLessEqual, D3D12_COMPARISON_FUNC_LESS_EQUAL);
        CASE(CompareOp::eGreater, D3D12_COMPARISON_FUNC_GREATER);
        CASE(CompareOp::eNotEqual, D3D12_COMPARISON_FUNC_NOT_EQUAL);
        CASE(CompareOp::eGreaterEqual, D3D12_COMPARISON_FUNC_GREATER_EQUAL);
        CASE(CompareOp::eAlways, D3D12_COMPARISON_FUNC_ALWAYS);
    default:
        return D3D12_COMPARISON_FUNC(0);
    }
};

D3D12_STENCIL_OP qhwi::d3d(StencilOp stencilOp)
{
    switch (stencilOp)
    {
        CASE(StencilOp::eKeep, D3D12_STENCIL_OP_KEEP);
        CASE(StencilOp::eZero, D3D12_STENCIL_OP_ZERO);
        CASE(StencilOp::eReplace, D3D12_STENCIL_OP_REPLACE);
        CASE(StencilOp::eIncrementAndClamp, D3D12_STENCIL_OP_INCR_SAT);
        CASE(StencilOp::eDecrementAndClamp, D3D12_STENCIL_OP_DECR_SAT);
        CASE(StencilOp::eInvert, D3D12_STENCIL_OP_INVERT);
        CASE(StencilOp::eIncrementAndWrap, D3D12_STENCIL_OP_INCR);
        CASE(StencilOp::eDecrementAndWrap, D3D12_STENCIL_OP_DECR);
    default:
        return D3D12_STENCIL_OP(0);
    }
};

D3D12_PRIMITIVE_TOPOLOGY qhwi::d3d(PrimitiveTopology primitiveTopology)
{
    switch (primitiveTopology)
    {
        CASE(PrimitiveTopology::ePointList, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        CASE(PrimitiveTopology::eLineList, D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        CASE(PrimitiveTopology::eLineStrip, D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
        CASE(PrimitiveTopology::eTriangleList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        CASE(PrimitiveTopology::eTriangleStrip, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    default:
        return D3D12_PRIMITIVE_TOPOLOGY(0);
    }
};

D3D12_PRIMITIVE_TOPOLOGY_TYPE qhwi::d3dPrimitiveTopologyType(PrimitiveTopology primitiveTopology)
{
    switch (primitiveTopology)
    {
        CASE(PrimitiveTopology::ePointList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
        CASE(PrimitiveTopology::eLineList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
        CASE(PrimitiveTopology::eLineStrip, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
        CASE(PrimitiveTopology::eTriangleList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        CASE(PrimitiveTopology::eTriangleStrip, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE(0);
    }
}

D3D12_INPUT_CLASSIFICATION qhwi::d3d(VertexInputRate vertexInputRate)
{
    switch (vertexInputRate)
    {
        CASE(VertexInputRate::eVertex, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA);
        CASE(VertexInputRate::eInstance, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA);
    default:
        return D3D12_INPUT_CLASSIFICATION(0);
    }
};

D3D12_COMMAND_LIST_TYPE qhwi::d3d(CommandListType commandListType)
{
    switch (commandListType)
    {
        CASE(CommandListType::ePrimary, D3D12_COMMAND_LIST_TYPE_DIRECT);
        CASE(CommandListType::eSecondary, D3D12_COMMAND_LIST_TYPE_BUNDLE);
    default:
        return D3D12_COMMAND_LIST_TYPE(0);
    }
}

D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE qhwi::d3d(AttachmentLoadOp op)
{
    switch (op)
    {
        CASE(AttachmentLoadOp::eClear, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR);
        CASE(AttachmentLoadOp::eDontCare, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD);
        CASE(AttachmentLoadOp::eNone, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS);
        CASE(AttachmentLoadOp::eLoad, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE);
        CASE(AttachmentLoadOp::ePreserveLocalRender, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE_LOCAL_RENDER);
        CASE(AttachmentLoadOp::ePreserveLocalSrv, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE_LOCAL_SRV);
        CASE(AttachmentLoadOp::ePreserveLocalUav, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE_LOCAL_UAV);
    default:
        return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE(0);
    }
}

D3D12_RENDER_PASS_ENDING_ACCESS_TYPE qhwi::d3d(AttachmentStoreOp op)
{
    switch (op)
    {
        CASE(AttachmentStoreOp::eDontCare, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD);
        CASE(AttachmentStoreOp::eNone, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
        CASE(AttachmentStoreOp::eStore, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
        CASE(AttachmentStoreOp::ePreserveLocalRender, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE_LOCAL_RENDER);
        CASE(AttachmentStoreOp::ePreserveLocalSrv, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE_LOCAL_SRV);
        CASE(AttachmentStoreOp::ePreserveLocalUav, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE_LOCAL_UAV);
    default:
        return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE(0);
    }
}

D3D12_COMMAND_LIST_TYPE qhwi::d3d(CommandQueueCapabilities caps)
{
    D3D12_COMMAND_LIST_TYPE type;
    if (caps & uint8_t(CommandQueueCapability_Graphics))
    {
        type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    else if (caps & uint8_t(CommandQueueCapability_Compute))
    {
        type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    }
    else
    {
        type = D3D12_COMMAND_LIST_TYPE_COPY;
    }
    return type;
}

D3D12_FILTER qhwi::d3d(Filter minFilter, Filter magFilter, Filter mipFilter, CompareOp c)
{
    static const D3D12_FILTER filterMap[] = { D3D12_FILTER_MIN_MAG_MIP_POINT,        D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT,
                                              D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR, D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT, D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
                                              D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_FILTER_MIN_MAG_MIP_LINEAR };

    static const D3D12_FILTER comparisonFilterMap[] = {
        D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,        D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR, D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
        D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT, D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
    };

    static_assert(ARRAYSIZE(filterMap) == ARRAYSIZE(comparisonFilterMap));
    uint8_t filterIndex = uint8_t(minFilter) << 2 | uint8_t(magFilter) << 1 | uint8_t(mipFilter);

    if (c == CompareOp::eNone)
    {
        return filterMap[filterIndex];
    }
    else
    {
        return comparisonFilterMap[filterIndex];
    }
}

D3D12_BARRIER_LAYOUT qhwi::d3dBarrierLayout(AccessModeMask accessMask, ImageLayout layout)
{
    switch (layout)
    {
        D3D12_BARRIER_LAYOUT_CASE(Undefined, UNDEFINED);
        D3D12_BARRIER_LAYOUT_CASE(General, COMMON);
        D3D12_BARRIER_LAYOUT_CASE(ColorResolveDest, RESOLVE_DEST);
        D3D12_BARRIER_LAYOUT_CASE(DepthResolveDest, RESOLVE_DEST);
        D3D12_BARRIER_LAYOUT_CASE(ColorAttachmentOptimal, RENDER_TARGET);
        D3D12_BARRIER_LAYOUT_CASE(DepthStencilReadOnlyOptimal, DEPTH_STENCIL_READ);
        D3D12_BARRIER_LAYOUT_CASE(DepthStencilAttachmentOptimal, DEPTH_STENCIL_WRITE);
        D3D12_BARRIER_LAYOUT_CASE(GenericRead, DIRECT_QUEUE_GENERIC_READ);
        D3D12_BARRIER_LAYOUT_CASE(UnorderedAccess, DIRECT_QUEUE_UNORDERED_ACCESS);
        D3D12_BARRIER_LAYOUT_CASE(Present, PRESENT);
        D3D12_BARRIER_LAYOUT_CASE(ShaderReadOnlyOptimal, DIRECT_QUEUE_SHADER_RESOURCE);
        D3D12_BARRIER_LAYOUT_CASE(TransferDstOptimal, DIRECT_QUEUE_COPY_DEST);
        D3D12_BARRIER_LAYOUT_CASE(TransferSrcOptimal, DIRECT_QUEUE_COPY_SOURCE);
    case ImageLayout::eLocalRead:
    {
        return (accessMask & AccessMode_ColorAttachmentWrite) ? D3D12_BARRIER_LAYOUT_RENDER_TARGET : D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
    }
    default:
        return D3D12_BARRIER_LAYOUT_COMMON;
    }
}

D3D12_RESOLVE_MODE qhwi::d3d(ResolveMode resolveMode)
{
    switch (resolveMode)
    {
        CASE(ResolveMode::eMin, D3D12_RESOLVE_MODE_MIN);
        CASE(ResolveMode::eMax, D3D12_RESOLVE_MODE_MAX);
        CASE(ResolveMode::eAverage, D3D12_RESOLVE_MODE_AVERAGE);
    default:
        return D3D12_RESOLVE_MODE_DECOMPRESS;
    }
}

D3D12_BARRIER_ACCESS qhwi::d3dBarrierAccess(PipelineStageMask pipelineStages, AccessModeMask accessModes)
{
    if (accessModes == AccessMode_None)
    {
        return D3D12_BARRIER_ACCESS_NO_ACCESS;
    }
    D3D12_BARRIER_ACCESS access = (D3D12_BARRIER_ACCESS)0;
    D3D12_BARRIER_ACCESS_CASE(IndirectCommandBufferRead, INDIRECT_ARGUMENT);
    D3D12_BARRIER_ACCESS_CASE(IndexBufferRead, INDEX_BUFFER);
    D3D12_BARRIER_ACCESS_CASE(VertexAttributeBufferRead, VERTEX_BUFFER);
    D3D12_BARRIER_ACCESS_CASE(ConstantBufferRead, CONSTANT_BUFFER);
    D3D12_BARRIER_ACCESS_CASE(ColorAttachmentReadWrite, RENDER_TARGET);
    if ((accessModes & AccessMode_DepthStencilAttachmentRead) && (!(accessModes & AccessMode_DepthStencilAttachmentWrite)))
    {
        //D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ is a read-only state
        access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
    }
    D3D12_BARRIER_ACCESS_CASE(DepthStencilAttachmentWrite, DEPTH_STENCIL_WRITE);
    D3D12_BARRIER_ACCESS_CASE(TransferRead, COPY_SOURCE);
    D3D12_BARRIER_ACCESS_CASE(TransferWrite, COPY_DEST);
    D3D12_BARRIER_ACCESS_CASE(ShaderSampledRead, SHADER_RESOURCE);
    if ((accessModes & AccessMode_ShaderStorageRead) && (!(accessModes & AccessMode_ShaderStorageWrite)))
    {
        access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    }
    D3D12_BARRIER_ACCESS_CASE(ShaderStorageWrite, UNORDERED_ACCESS);
    if (pipelineStages & PipelineStage_AccelerationStructureBuild)
    {
        if (accessModes & AccessMode_ShaderRead)
        {
            // Workaround for https://github.com/microsoft/DirectX-Specs/issues/147
            accessModes = AccessMode_AccelerationStructureRead;
        }
    }
    D3D12_BARRIER_ACCESS_CASE(AccelerationStructureRead, RAYTRACING_ACCELERATION_STRUCTURE_READ);
    D3D12_BARRIER_ACCESS_CASE(AccelerationStructureWrite, RAYTRACING_ACCELERATION_STRUCTURE_WRITE);
    D3D12_BARRIER_ACCESS_CASE(ColorResolveDest, RESOLVE_DEST);
    D3D12_BARRIER_ACCESS_CASE(DepthResolveDest, RESOLVE_DEST);
    return access;
}

D3D12_BARRIER_SYNC qhwi::d3dBarrierSync(PipelineStageMask pipelineStages, AccessModeMask accessModes)
{
    if (accessModes & AccessMode_ColorResolveDest || accessModes & AccessMode_DepthResolveDest)
    {
        return D3D12_BARRIER_SYNC_RESOLVE;
    }

    // TODO: Logic needs to be improved.(This fix is for texture clear() API.)
    if (pipelineStages == PipelineStage_Transfer && accessModes & AccessMode_UnorderedAccess)
    {
        return D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
    }

    constexpr int PipelineStage_NonPixelShader =
        (qhwi::PipelineStage_VertexShader | qhwi::PipelineStage_HullShader | qhwi::PipelineStage_DomainShader | qhwi::PipelineStage_GeometryShader |
         qhwi::PipelineStage_ComputeShader | qhwi::PipelineStage_AmplificationShader | qhwi::PipelineStage_MeshShader);

    if (pipelineStages == PipelineStage_None)
    {
        return D3D12_BARRIER_SYNC_NONE;
    }
    D3D12_BARRIER_SYNC sync = (D3D12_BARRIER_SYNC)0;
    D3D12_BARRIER_SYNC_CASE(DrawIndirect, EXECUTE_INDIRECT);
    D3D12_BARRIER_SYNC_CASE(DispatchIndirect, EXECUTE_INDIRECT);
    D3D12_BARRIER_SYNC_CASE(PixelShader, PIXEL_SHADING);
    D3D12_BARRIER_SYNC_CASE(NonPixelShader, NON_PIXEL_SHADING);
    D3D12_BARRIER_SYNC_CASE(ColorAttachmentOutput, RENDER_TARGET);
    D3D12_BARRIER_SYNC_CASE(ComputeShader, COMPUTE_SHADING);
    D3D12_BARRIER_SYNC_CASE(Transfer, COPY);
    D3D12_BARRIER_SYNC_CASE(AllGraphics, DRAW);
    D3D12_BARRIER_SYNC_CASE(AllCommands, ALL);
    D3D12_BARRIER_SYNC_CASE(VertexInput, ALL_SHADING);
    D3D12_BARRIER_SYNC_CASE(IndexInput, INDEX_INPUT);
    D3D12_BARRIER_SYNC_CASE(AccelerationStructureBuild, BUILD_RAYTRACING_ACCELERATION_STRUCTURE);
    D3D12_BARRIER_SYNC_CASE(EarlyPixelTests, ALL);
    D3D12_BARRIER_SYNC_CASE(LatePixelTests, ALL);
    D3D12_BARRIER_SYNC_CASE(BottomOfPipe, ALL);
    D3D12_BARRIER_SYNC_CASE(RayTracingShader, ALL);
    D3D12_BARRIER_SYNC_CASE(Clear, CLEAR_UNORDERED_ACCESS_VIEW);
    return sync;
}

D3D12_SHADER_VISIBILITY qhwi::d3dShaderVisibility(ShaderStageMask shaderStages)
{
    switch (shaderStages)
    {
        CASE(ShaderStage_Vertex, D3D12_SHADER_VISIBILITY_VERTEX);
        CASE(ShaderStage_Pixel, D3D12_SHADER_VISIBILITY_PIXEL);
        CASE(ShaderStage_Hull, D3D12_SHADER_VISIBILITY_HULL);
        CASE(ShaderStage_Domain, D3D12_SHADER_VISIBILITY_DOMAIN);
        CASE(ShaderStage_Geometry, D3D12_SHADER_VISIBILITY_GEOMETRY);
        CASE(ShaderStage_Amplification, D3D12_SHADER_VISIBILITY_AMPLIFICATION);
        CASE(ShaderStage_Mesh, D3D12_SHADER_VISIBILITY_MESH);
    default:
        return D3D12_SHADER_VISIBILITY_ALL;
    }
}

DXGI_USAGE qhwi::dxgiUsage(TextureUsageMask mask)
{
    DXGI_USAGE usage = 0;
    DXGI_USAGE_FLAG(ColorAttachment, RENDER_TARGET_OUTPUT);
    DXGI_USAGE_FLAG(DepthStencilAttachment, RENDER_TARGET_OUTPUT);
    DXGI_USAGE_FLAG(Sampled, SHADER_INPUT);
    DXGI_USAGE_FLAG(Storage, UNORDERED_ACCESS);
    return usage;
}

D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS qhwi::d3dRaytracingAccelerationStructureBuildFlags(AccelerationStructureBuildMode mode, AccelerationStructureBuildFlags flags)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS r = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    if (flags & AccelerationStructureBuildFlag_AllowUpdate)
    {
        r |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    }
    if (flags & AccelerationStructureBuildFlag_AllowCompaction)
    {
        r |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
    }
    if (flags & AccelerationStructureBuildFlag_PreferFastTrace)
    {
        r |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    }
    if (flags & AccelerationStructureBuildFlag_PreferFastBuild)
    {
        r |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    }
    if (flags & AccelerationStructureBuildFlag_MinimizeMemory)
    {
        r |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
    }
    if (mode == AccelerationStructureBuildMode::eUpdate)
    {
        r |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    }
    return r;
}

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
DSTORAGE_REQUEST_SOURCE_TYPE qhwi::compressedstorage::dstorageRequestSourceType(qhwi::compressedstorage::StorageRequestSourceType sourceType)
{
    switch (sourceType)
    {
        CASE(qhwi::compressedstorage::StorageRequestSourceType::eFile, DSTORAGE_REQUEST_SOURCE_FILE);
        CASE(qhwi::compressedstorage::StorageRequestSourceType::eMemory, DSTORAGE_REQUEST_SOURCE_MEMORY);
    default:
        return DSTORAGE_REQUEST_SOURCE_TYPE(0);
    }
}

DSTORAGE_REQUEST_DESTINATION_TYPE qhwi::compressedstorage::dstorageRequestDestinationType(qhwi::compressedstorage::StorageRequestDestinationType destinationType)
{
    switch (destinationType)
    {
        CASE(qhwi::compressedstorage::StorageRequestDestinationType::eMemory, DSTORAGE_REQUEST_DESTINATION_MEMORY);
        CASE(qhwi::compressedstorage::StorageRequestDestinationType::eBuffer, DSTORAGE_REQUEST_DESTINATION_BUFFER);
        CASE(qhwi::compressedstorage::StorageRequestDestinationType::eTextureRegion, DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION);
        CASE(qhwi::compressedstorage::StorageRequestDestinationType::eMutipleSubresources, DSTORAGE_REQUEST_DESTINATION_MULTIPLE_SUBRESOURCES);
        CASE(qhwi::compressedstorage::StorageRequestDestinationType::eTiles, DSTORAGE_REQUEST_DESTINATION_TILES);
    default:
        return DSTORAGE_REQUEST_DESTINATION_TYPE(0);
    }
}

DSTORAGE_PRIORITY qhwi::compressedstorage::dstoragePriority(qhwi::compressedstorage::StoragePriority priority)
{
    switch (priority)
    {
        CASE(qhwi::compressedstorage::StoragePriority::eLow, DSTORAGE_PRIORITY_LOW);
        CASE(qhwi::compressedstorage::StoragePriority::eNormal, DSTORAGE_PRIORITY_NORMAL);
        CASE(qhwi::compressedstorage::StoragePriority::eHigh, DSTORAGE_PRIORITY_HIGH);
        CASE(qhwi::compressedstorage::StoragePriority::eRealtime, DSTORAGE_PRIORITY_REALTIME);
    default:
        return DSTORAGE_PRIORITY(0);
    }
}

DSTORAGE_COMPRESSION_FORMAT qhwi::compressedstorage::dstorageCompressionFormat(qhwi::compressedstorage::CompressionFormat format)
{
    switch (format)
    {
        CASE(qhwi::compressedstorage::CompressionFormat::eNone, DSTORAGE_COMPRESSION_FORMAT_NONE);
        CASE(qhwi::compressedstorage::CompressionFormat::eGDeflate, DSTORAGE_COMPRESSION_FORMAT_GDEFLATE);
        CASE(qhwi::compressedstorage::CompressionFormat::eCustom, DSTORAGE_CUSTOM_COMPRESSION_0);
    default:
        return DSTORAGE_COMPRESSION_FORMAT(0);
    }
}
#endif // QHWI_ENABLE_COMPRESSED_STORAGE
