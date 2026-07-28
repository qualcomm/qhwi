//
//
//  qhwi_enums.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __QHWI_ENUMS_H__
#define __QHWI_ENUMS_H__

#include <cstdint>

namespace qhwi
{
enum class GraphicsBackend : uint32_t
{
    eAuto,
    eDirectX12,
    eVulkan,
};

enum class WindowBackend : uint32_t
{
    eAuto,
    eGlfw,
    eAndroid,
};

namespace ext
{
enum class GpuCaptureBackend
{
    eAuto = 0,
#ifdef QHWI_GPU_CAPTURE_BACKEND_PIX
    ePix = 1 << 1,
#endif
};
} // namespace ext

enum class CommandListType : uint8_t
{
    ePrimary,
    eSecondary
};

enum CommandQueueCapability
{
    CommandQueueCapability_Graphics = 1 << 0,
    CommandQueueCapability_Compute = 1 << 1,
    CommandQueueCapability_Transfer = 1 << 2,
    CommandQueueCapability_OpticalFlow = 1 << 3,
    CommandQueueCapability_DataGraph = 1 << 4
};

typedef uint8_t CommandQueueCapabilities;

enum class IndexFormat : uint8_t
{
    eUint16,
    eUint32
};

// Note: Keep FormatInfo lookup table in sync with qhwi::Format, in the exact same order
enum class Format : uint32_t
{
    eUnknown,
    eR8Unorm,
    eRg8Unorm,
    eRgba8Unorm,
    eR8Int,
    eRg8Int,
    eRgba8Int,
    eR8Uint,
    eRg8Uint,
    eRgba8Uint,
    eR16Unorm,
    eRg16Unorm,
    eRgba16Unorm,
    eR16Int,
    eRg16Int,
    eRgba16Int,
    eR16Uint,
    eRg16Uint,
    eRgba16Uint,
    eR16Float,
    eRg16Float,
    eRgba16Float,
    eR32Int,
    eRg32Int,
    eRgba32Int,
    eR32Uint,
    eRg32Uint,
    eRgba32Uint,
    eR32Float,
    eRg32Float,
    eRgba32Float,
    eR8Snorm,
    eRg8Snorm,
    eRgba8Snorm,
    eR16Snorm,
    eRg16Snorm,
    eRgba16Snorm,
    eRgb32Float,
    eBgra8Unorm,
    eRgba8Srgb,
    eBgra8Srgb,
    eRgb9E5Float,
    eRgb10A2Unorm,
    eRg11B10Float,
    eBc1Unorm,
    eBc1UnormSrgb,
    eBc2Unorm,
    eBc2UnormSrgb,
    eBc3Unorm,
    eBc3UnormSrgb,
    eBc4Unorm,
    eBc4Snorm,
    eBc5Unorm,
    eBc5Snorm,
    eBc6HUFloat,
    eBc6HSFloat,
    eBc7Unorm,
    eBc7UnormSrgb,
    eEtc2RgbUnorm,
    eEtc2RgbSrgb,
    eEtc2RgbaUnorm,
    eEtc2RgbaSrgb,
    eEacR11Unorm,
    eEacRg11Unorm,
    eAstc4x4Unorm,
    eAstc4x4Srgb,
    eAstc5x4Unorm,
    eAstc5x4Srgb,
    eAstc5x5Unorm,
    eAstc5x5Srgb,
    eAstc6x5Unorm,
    eAstc6x5Srgb,
    eAstc6x6Unorm,
    eAstc6x6Srgb,
    eAstc8x5Unorm,
    eAstc8x5Srgb,
    eAstc8x6Unorm,
    eAstc8x6Srgb,
    eAstc8x8Unorm,
    eAstc8x8Srgb,
    eAstc10x5Unorm,
    eAstc10x5Srgb,
    eAstc10x6Unorm,
    eAstc10x6Srgb,
    eAstc10x8Unorm,
    eAstc10x8Srgb,
    eAstc10x10Unorm,
    eAstc10x10Srgb,
    eAstc12x10Unorm,
    eAstc12x10Srgb,
    eAstc12x12Unorm,
    eAstc12x12Srgb,
    eD16Unorm,
    eD24UnormS8Uint,
    eD32Float,
    eD32FloatS8Uint,
    eRg16S10_5_NV,
    eCount
};

enum class ComponentSwizzle : uint8_t
{
    eR,
    eG,
    eB,
    eA,
    eOne,
    eZero
};

enum Feature
{
    Feature_ConservativeRasterization = 1 << 0,
    Feature_TextureCompressionBc = 1 << 1,
    Feature_TextureCompressionEtc = 1 << 2,
    Feature_TextureCompressionAstc = 1 << 3,
    Feature_MeshShaders = 1 << 4,
    Feature_RayQuery = 1 << 5,
    Feature_RayPipelines = 1 << 6,
    Feature_ShaderFloat16 = 1 << 7,
    Feature_HdrMetadata = 1 << 8,
    Feature_Barycentric = 1 << 9,
    Feature_StorageQueue = 1 << 10,
    Feature_RtTraceRaysIndirect = 1 << 11,
    Feature_RtInvocationReorder = 1 << 12,
    Feature_OpacityMicromaps = 1 << 13,
    Feature_CustomResolve = 1 << 14
};

typedef uint32_t FeaturesMask;

enum class FormatKind : uint8_t
{
    eInteger,
    eNormalized,
    eFloat,
    eDepthStencil
};

enum class BlendOp : uint8_t
{
    eAdd,
    eSubtract,
    eReverseSubtract,
    eMin,
    eMax
};

enum class BlendFactor : uint8_t
{
    eZero,
    eOne,
    eSrcColor,
    eOneMinusSrcColor,
    eDstColor,
    eOneMinusDstColor,
    eSrcAlpha,
    eOneMinusSrcAlpha,
    eDstAlpha,
    eOneMinusDstAlpha,
    eConstantColor,
    eOneMinusConstantColor,
    eSrcAlphaSaturate,
    eSrc1Color,
    eOneMinusSrc1Color,
    eSrc1Alpha,
    eOneMinusSrc1Alpha
};

enum class LogicOp : uint8_t
{
    eClear,
    eAnd,
    eAndReverse,
    eCopy,
    eAndInverted,
    eNoOp,
    eXor,
    eOr,
    eNor,
    eEquivalent,
    eInvert,
    eOrReverse,
    eCopyInverted,
    eOrInverted,
    eNand,
    eSet
};

enum class ColorChannel : uint8_t
{
    eR = 1 << 0,
    eG = 1 << 1,
    eB = 1 << 2,
    eA = 1 << 3,
    eAll = eR | eG | eB | eA
};
typedef uint8_t ColorWriteMask;

enum class TextureType : uint8_t
{
    eUndefined,
    e1D,
    e2D,
    e3D,
    eCube,
    e1DArray,
    e2DArray,
    eCubeArray
};

enum class SamplerAddressMode : uint8_t
{
    eRepeat,
    eMirroredRepeat,
    eClampToEdge,
    eClampToBorder,
    eMirrorClampToEdge
};

enum SamplerCreateFlag
{
    SamplerCreateFlag_None,
    SamplerCreateFlag_ImageProcessingQCOM = 1 << 0
};

typedef uint8_t SamplerCreateFlags;

enum class SamplerReductionMode : uint8_t
{
    eNone,
    eWeightedAverage,
    eMin,
    eMax
};

enum class FillMode : uint8_t
{
    eSolid,
    eWireframe
};
enum class CullMode : uint8_t
{
    eNone,
    eFront,
    eBack
};

enum class CompareOp : uint8_t
{
    eNone,
    eNever,
    eLess,
    eEqual,
    eLessEqual,
    eGreater,
    eNotEqual,
    eGreaterEqual,
    eAlways
};

enum class StencilOp : uint8_t
{
    eKeep,
    eZero,
    eReplace,
    eIncrementAndClamp,
    eDecrementAndClamp,
    eInvert,
    eIncrementAndWrap,
    eDecrementAndWrap
};

enum class PrimitiveTopology : uint8_t
{
    ePointList,
    eLineList,
    eLineStrip,
    eTriangleList,
    eTriangleStrip
};

enum class PrimitiveRestartValue : uint8_t
{
    eFFFF,
    eFFFFFFFF
};

enum class VertexInputRate : uint8_t
{
    eVertex,
    eInstance
};

//TODO: support bicubic
enum class Filter : uint8_t
{
    eNearest,
    eLinear
};

enum class DescriptorType : uint8_t
{
    eConstantBuffer,
    eDynamicConstantBuffer,
    eByteAddressBuffer,
    eRWByteAddressBuffer,
    eStructuredBuffer,
    eRWStructuredBuffer,
    eStorageBuffer = eRWStructuredBuffer,
    eTypedBuffer,
    eRWTypedBuffer,
    eTexture,
    eRWTexture,
    eStorageImage = eRWTexture,
    eSampler,
    eInputAttachment,
    eAccelerationStructure,
    eBlockMatchImageQCOM,
    eSampleWeightImageQCOM,
    eCount
};

constexpr bool isBufferDescriptorType(DescriptorType t)
{
    switch (t)
    {
    case DescriptorType::eConstantBuffer:
    case DescriptorType::eDynamicConstantBuffer:
    case DescriptorType::eByteAddressBuffer:
    case DescriptorType::eRWByteAddressBuffer:
    case DescriptorType::eStructuredBuffer:
    case DescriptorType::eRWStructuredBuffer:
    case DescriptorType::eTypedBuffer:
    case DescriptorType::eRWTypedBuffer:
        return true;
    default:
        return false;
    }
}

constexpr bool isTextureDescriptorType(DescriptorType t)
{
    switch (t)
    {
    case DescriptorType::eTexture:
    case DescriptorType::eRWTexture:
        return true;
    default:
        return false;
    }
}

enum class BindingOffset : uint8_t
{
    eBShift,
    eTShift,
    eSShift,
    eUShift,
    eCount
};

enum class HeapType : uint8_t
{
    eDefault,
    eUpload,
    eShared = eUpload,
    eDownload
};

enum HeapFlag : uint8_t
{
    HeapFlag_None = 0,
    HeapFlag_Shared = 1
};

typedef uint32_t HeapFlags;

enum BufferUsage
{
    BufferUsage_VertexBuffer = 1 << 0,
    BufferUsage_IndexBuffer = 1 << 1,
    BufferUsage_ConstantBuffer = 1 << 2,
    BufferUsage_ByteAddressBuffer = 1 << 3,
    BufferUsage_RWByteAddressBuffer = 1 << 4,
    BufferUsage_StructuredBuffer = 1 << 5,
    BufferUsage_RWStructuredBuffer = 1 << 6,
    BufferUsage_TypedBuffer = 1 << 7,
    BufferUsage_RWTypedBuffer = 1 << 8,
    BufferUsage_ShaderResource = BufferUsage_ConstantBuffer | BufferUsage_ByteAddressBuffer | BufferUsage_StructuredBuffer | BufferUsage_TypedBuffer,
    BufferUsage_UnorderedAccess = BufferUsage_RWByteAddressBuffer | BufferUsage_RWStructuredBuffer | BufferUsage_RWTypedBuffer,
    BufferUsage_StorageBuffer = BufferUsage_UnorderedAccess,
    BufferUsage_AccelerationStructure = 1 << 9,
    BufferUsage_AccelerationStructureBuildSource = 1 << 10,
    BufferUsage_IndirectBuffer = 1 << 11,
    BufferUsage_ShaderBindingTable = 1 << 12,
    BufferUsage_TileMemoryQCOM = 1 << 13,
};

typedef uint32_t BufferUsageMask;

enum TextureUsage
{
    TextureUsage_Sampled = 1 << 0,
    TextureUsage_ColorAttachment = 1 << 1,
    TextureUsage_DepthStencilAttachment = 1 << 2,
    TextureUsage_TransientAttachment = 1 << 3,
    TextureUsage_InputAttachment = 1 << 4,
    TextureUsage_Storage = 1 << 5,
    TextureUsage_SampleBlockMatchQCOM = 1 << 6,
    TextureUsage_SampleWeightQCOM = 1 << 7,
    TextureUsage_OpticalFlowInput = 1 << 8,
    TextureUsage_OpticalFlowOutput = 1 << 9,
    TextureUsage_TileMemoryQCOM = 1 << 10
};

typedef uint32_t TextureUsageMask;

enum AccessMode
{
    AccessMode_None = 0,
    AccessMode_IndirectCommandBufferRead = 1 << 0,
    AccessMode_IndexBufferRead = 1 << 1,
    AccessMode_VertexAttributeBufferRead = 1 << 2,
    AccessMode_ConstantBufferRead = 1 << 3,
    AccessMode_InputAttachmentRead = 1 << 4,
    AccessMode_ColorAttachmentRead = 1 << 5,
    AccessMode_ColorAttachmentWrite = 1 << 6,
    AccessMode_ColorAttachmentReadWrite = AccessMode_ColorAttachmentRead | AccessMode_ColorAttachmentWrite,
    AccessMode_DepthStencilAttachmentRead = 1 << 7,
    AccessMode_DepthStencilAttachmentWrite = 1 << 8,
    AccessMode_DepthStencilAttachmentReadWrite = AccessMode_DepthStencilAttachmentRead | AccessMode_DepthStencilAttachmentWrite,
    AccessMode_TransferRead = 1 << 9,
    AccessMode_TransferWrite = 1 << 10,
    AccessMode_TransferReadWrite = AccessMode_TransferRead | AccessMode_TransferWrite,
    AccessMode_HostRead = 1 << 11,
    AccessMode_HostWrite = 1 << 12,
    AccessMode_HostReadWrite = AccessMode_HostRead | AccessMode_HostWrite,
    AccessMode_MemoryRead = 1 << 13,
    AccessMode_MemoryWrite = 1 << 14,
    AccessMode_MemoryReadWrite = AccessMode_MemoryRead | AccessMode_MemoryWrite,
    AccessMode_ShaderSampledRead = 1 << 15,
    AccessMode_Sample = AccessMode_ShaderSampledRead,
    AccessMode_ShaderStorageRead = 1 << 16,
    AccessMode_StorageRead = AccessMode_ShaderStorageRead,
    AccessMode_ShaderStorageWrite = 1 << 17,
    AccessMode_StorageWrite = AccessMode_ShaderStorageWrite,
    AccessMode_ShaderStorageReadWrite = (AccessMode_ShaderStorageRead | AccessMode_ShaderStorageWrite),
    AccessMode_StorageReadWrite = AccessMode_ShaderStorageReadWrite,
    AccessMode_UnorderedAccess = AccessMode_ShaderStorageReadWrite,
    AccessMode_AccelerationStructureRead = 1 << 18,
    AccessMode_AccelerationStructureWrite = 1 << 19,
    AccessMode_AccelerationStructureReadWrite = AccessMode_AccelerationStructureRead | AccessMode_AccelerationStructureWrite,
    AccessMode_ShaderRead = AccessMode_ConstantBufferRead | AccessMode_ShaderSampledRead | AccessMode_ShaderStorageRead,
    AccessMode_ColorResolveDest = 1 << 20,
    AccessMode_DepthResolveDest = 1 << 21,
    AccessMode_OpticalFlowRead = 1 << 22,
    AccessMode_OpticalFlowWrite = 1 << 23,
    AccessMode_TileAttachmentReadQcom = 1 << 24,
    AccessMode_TileAttachmentWriteQcom = 1 << 25,
    AccessMode_TileTransientReadQcom = 1 << 26,
    AccessMode_TileTransientWriteQcom = 1 << 27,
    AccessMode_TileTransientReadWriteQcom = AccessMode_TileTransientReadQcom | AccessMode_TileTransientWriteQcom
};

typedef uint32_t AccessModeMask;

enum PipelineStage
{
    PipelineStage_None = 0,
    PipelineStage_TopOfPipe = 1 << 0,
    PipelineStage_DrawIndirect = 1 << 1,
    PipelineStage_DispatchIndirect = PipelineStage_DrawIndirect,
    PipelineStage_VertexInput = 1 << 2,
    PipelineStage_VertexShader = 1 << 3,
    PipelineStage_HullShader = 1 << 4,
    PipelineStage_DomainShader = 1 << 5,
    PipelineStage_GeometryShader = 1 << 6,
    PipelineStage_PixelShader = 1 << 7,
    PipelineStage_EarlyPixelTests = 1 << 8,
    PipelineStage_LatePixelTests = 1 << 9,
    PipelineStage_ColorAttachmentOutput = 1 << 10,
    PipelineStage_ComputeShader = 1 << 11,
    PipelineStage_Transfer = 1 << 12,
    PipelineStage_BottomOfPipe = 1 << 13,
    PipelineStage_Host = 1 << 14,
    PipelineStage_AllGraphics = 1 << 15,
    PipelineStage_AllCommands = 1 << 16,
    PipelineStage_IndexInput = 1 << 17,
    PipelineStage_AmplificationShader = 1 << 18,
    PipelineStage_MeshShader = 1 << 19,
    PipelineStage_AccelerationStructureBuild = 1 << 20,
    PipelineStage_OpticalFlow = 1 << 21,
    PipelineStage_RayTracingShader = 1 << 22,
    PipelineStage_Clear = 1 << 23,
    PipelineStage_VertexOrPixelShader = PipelineStage_VertexShader | PipelineStage_PixelShader
};

typedef uint32_t PipelineStageMask;

enum class ImageLayout : uint8_t
{
    eUndefined,
    eGeneral,
    eGenericRead,
    eUnorderedAccess,
    eColorResolveDest,
    eDepthResolveDest,
    eColorAttachmentOptimal,
    eDepthStencilAttachmentOptimal,
    eDepthStencilReadOnlyOptimal,
    eShaderReadOnlyOptimal,
    eTransferSrcOptimal,
    eTransferDstOptimal,
    ePresent,
    eLocalRead
};

enum class ShaderType
{
    eUnknown,
    eVertex,
    ePixel,
    eCompute,
    eDomain,
    eHull,
    eGeometry,
    eAmplification,
    eMesh,
    eRayGeneration,
    eAnyHit,
    eClosestHit,
    eMiss,
    eIntersection,
    eCallable,
    eCount
};

enum class RayTracingShaderGroupType
{
    eGeneral,
    eTriangleHit,
    eProceduralHit
};

enum ShaderStage
{
    ShaderStage_Vertex = 1 << 0,
    ShaderStage_Pixel = 1 << 1,
    ShaderStage_Compute = 1 << 2,
    ShaderStage_Domain = 1 << 3,
    ShaderStage_Hull = 1 << 4,
    ShaderStage_Geometry = 1 << 5,
    ShaderStage_Amplification = 1 << 6,
    ShaderStage_Mesh = 1 << 7,
    ShaderStage_RayGeneration = 1 << 8,
    ShaderStage_AnyHit = 1 << 9,
    ShaderStage_ClosestHit = 1 << 10,
    ShaderStage_Miss = 1 << 11,
    ShaderStage_Intersection = 1 << 12,
    ShaderStage_Callable = 1 << 13,

    ShaderStage_All = (ShaderStage_Callable << 1) - 1
};

typedef uint16_t ShaderStageMask;

enum DescriptorBindingFlag
{
    DescriptorBindingFlag_UpdateAfterBind = 1 << 0,
    DescriptorBindingFlag_PartiallyBound = 1 << 1,
    DescriptorBindingFlag_VariableDescriptorCount = 1 << 2
};

typedef uint8_t DescriptorBindingFlags;

enum class PipelineBindPoint : uint8_t
{
    eGraphics,
    eCompute,
    eRayTracing
};

enum class AttachmentLoadOp : uint8_t
{
    eDontCare,
    eNone,
    eClear,
    eLoad,
    ePreserveLocalRender,
    ePreserveLocalSrv,
    ePreserveLocalUav
};

enum class AttachmentStoreOp : uint8_t
{
    eDontCare,
    eNone,
    eStore,
    ePreserveLocalRender,
    ePreserveLocalSrv,
    ePreserveLocalUav
};

enum class ResolveMode : uint8_t
{
    eNone,
    eAverage,
    eMin,
    eMax
};

enum FenceState
{
    FenceState_Unsignaled,
    FenceState_Signaled
};

enum class TimerState : uint8_t
{
    eReset,
    eStarted,
    eStopped,
    eUnresolved,
    eResolved
};

enum class SampleCount : uint8_t
{
    e1 = 1,
    e2 = 2,
    e4 = 4,
    e8 = 8,
    e16 = 16,
    e32 = 32,
    e64 = 64
};

enum class ResultCode : uint8_t
{
    eSuccess,
    eTimeout,
    eUnsupported,
    eErrorOther
};

enum DependencyFlag
{
    DependencyFlag_ByRegion,
    DependencyFlag_DeviceGroup,
    DependencyFlag_ViewLocal
};

typedef uint8_t DependencyFlags;

enum class ClearValueType : uint8_t
{
    eColor,
    eDepthStencil
};

enum FormatSupport
{
    FormatSupport_None = 0,
    FormatSupport_Buffer = 1 << 0,
    FormatSupport_IndexBuffer = 1 << 1,
    FormatSupport_VertexBuffer = 1 << 2,
    FormatSupport_StorageImage = 1 << 3,
    // note: the format does not necessarily support
    // both depth and stencil, it supports either depth,
    // or stencil, or both
    FormatSupport_DepthStencilAttachment = 1 << 4,
    FormatSupport_SampledImage = 1 << 5,
    FormatSupport_ColorAttachment = 1 << 6,
    FormatSupport_OpticalFlowImage = 1 << 7,
    FormatSupport_OpticalFlowVector = 1 << 8,
    FormatSupport_OpticalFlowCost = 1 << 9
};

typedef uint32_t FormatSupportMask;

enum AccelerationStructureBuildFlag
{
    AccelerationStructureBuildFlag_None,
    AccelerationStructureBuildFlag_AllowUpdate = 1 << 0,
    AccelerationStructureBuildFlag_AllowCompaction = 1 << 1,
    AccelerationStructureBuildFlag_PreferFastTrace = 1 << 2,
    AccelerationStructureBuildFlag_PreferFastBuild = 1 << 3,
    AccelerationStructureBuildFlag_MinimizeMemory = 1 << 4
};

typedef uint8_t AccelerationStructureBuildFlags;

enum class AccelerationStructureBuildMode
{
    eBuild,
    eUpdate
};

#ifdef QHWI_ENABLE_COMPRESSED_STORAGE
namespace compressedstorage
{
enum class StorageRequestSourceType : uint8_t
{
    eFile,
    eMemory
};

enum class StorageRequestDestinationType : uint8_t
{
    eMemory,
    eBuffer,
    eTextureRegion,
    eMutipleSubresources,
    eTiles
};

enum class StoragePriority : uint8_t
{
    eLow,
    eNormal,
    eHigh,
    eRealtime
};

enum class CompressionFormat : uint8_t
{
    eNone,
    eGDeflate,
    eCustom
};

} // namespace compressedstorage
#endif // QHWI_ENABLE_COMPRESSED_STORAGE

} // namespace qhwi

#endif
