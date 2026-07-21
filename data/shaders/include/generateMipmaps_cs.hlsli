//
//
//  generateMipmaps_cs.hlsli
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifndef __GENERATEMIPMAPS_CS_HLSLI__
# define __GENERATEMIPMAPS_CS_HLSLI__

# include "qhwiHlslGlue.hlsli"

# define MAX_ARRAY_LAYERS 6
# define MAX_MIP_LEVELS  16

#define NUM_THREAD_X   8U
#define NUM_THREAD_Y   8U 

enum MipmapTextureUsage
{
    MipmapTextureUsage_Generic,
    MipmapTextureUsage_SRGB,
    MipmapTextureUsage_TangentSpaceNormal,
    MipmapTextureUsage_LocalSpaceNormal
};

enum MipmapRegisterSpace
{
    MipmapRegisterSpace_Textures,
    MipmapRegisterSpace_Buffers,
    MipmapRegisterSpace_Count
};

enum MipmapFilter
{
    MipmapFilter_Nearest,
    MipmapFilter_Linear,
    MipmapFilter_Min,
    MipmapFilter_Max,
    MipmapFilter_Count
};

struct MipmapConfig
{
    uint mipmapTextureUsage;
    uint dstMipLevel;
    uint targetCounterValue;
    uint numLayers;
    uint2 kernelSize;
    uint2 srcSize;
    uint2 dstSize;
    uint mipmapFilter;
};

#ifdef __HLSL_VERSION
#ifndef NUM_CHANNELS
#error "NUM_CHANNELS must be defined as 1, 2, 3, or 4 before including this file."
#endif

#if NUM_CHANNELS == 1
#define floatN float
#define MAX_VAL float(1.0e+30);
#elif NUM_CHANNELS == 2
#define floatN float2
#define MAX_VAL float2(1.0e+30, 1.0e+30);
#elif NUM_CHANNELS == 3
#define floatN float3
#define MAX_VAL float3(1.0e+30, 1.0e+30, 1.0e+30);
#elif NUM_CHANNELS == 4
#define floatN float4
#define MAX_VAL float4(1.0e+30, 1.0e+30, 1.0e+30, 1.0e+30);
#else
    #error "Unsupported NUM_CHANNELS value"
#endif

# define SHADER_REGISTER_SPACE(s) space ## s

# define SHADER_REGISTER_SPACE_TEXTURES   SHADER_REGISTER_SPACE(0)
# define SHADER_REGISTER_SPACE_BUFFERS    SHADER_REGISTER_SPACE(1)

# define LOG2I(n) firstbithigh(n)

[[vk::push_constant]]  ConstantBuffer<MipmapConfig> config : register(b0);
RWTexture2D<floatN> texture[MAX_MIP_LEVELS * MAX_ARRAY_LAYERS] : register(u0, SHADER_REGISTER_SPACE_TEXTURES);
globallycoherent RWStructuredBuffer<uint> atomicCounter : register(u0, SHADER_REGISTER_SPACE_BUFFERS);

// Utils:

floatN LinearToSrgb(floatN inLinear)
{
    // https://color.org/chardata/rgb/sRGB.pdf
#if NUM_CHANNELS < 3
    return inLinear;
#elif NUM_CHANNELS == 3
    return select(inLinear <= 0.0031308, 12.92 * inLinear, 1.055 * pow(inLinear, (1.0 / 2.4)) - 0.055);
#else // 4 channels
    float3 srgb = select(inLinear.xyz <= 0.0031308, 12.92 * inLinear.xyz, 1.055 * pow(inLinear.xyz, (1.0 / 2.4)) - 0.055);
    return float4(srgb, inLinear.w);
#endif
}

floatN SrgbToLinear(floatN inSRGB)
{
    // https://color.org/chardata/rgb/sRGB.pdf
#if NUM_CHANNELS < 3
    return inSRGB;
#elif NUM_CHANNELS == 3
    return select(inSRGB <= 0.04045, inSRGB / 12.92, pow((inSRGB + 0.055) / 1.055, 2.4));
#else // 4 channels
    float3 lin = select(inSRGB.xyz <= 0.04045, inSRGB.xyz / 12.92, pow((inSRGB.xyz + 0.055) / 1.055, 2.4));
    return float4(lin, inSRGB.w);
#endif
}

// Returns the coordinates of target pixel in source texture.
uint2 computeSourceLocationFromDestination(uint2 dstLocation)
{
    float2 srcCoordF = (float2(dstLocation) + 0.5) * 2.0;
    return uint2(srcCoordF);
}

// Returns the top left corner coordinates of the kernel to be traversed in source texture.
int2 computeSourceLocationFromDestinationTopLeft(uint2 dstLocation)
{
    float2 srcCoordF = (float2(dstLocation) + 0.5) * 2.0;
    return int2(srcCoordF - float2(config.kernelSize.x, config.kernelSize.y) * 0.5);
}

floatN minColor(floatN a, floatN b)
{
#if NUM_CHANNELS == 1
    return b < a ? b : a;
#elif NUM_CHANNELS > 1 && NUM_CHANNELS < 5
    float aMagSq = dot(a, a);
    float bMagSq = dot(b, b);
    return bMagSq < aMagSq ? b : a;
#else
    return (floatN)0;
#endif
}

floatN maxColor(floatN a, floatN b)
{
#if NUM_CHANNELS == 1
    return b > a ? b : a;
#elif NUM_CHANNELS > 1 && NUM_CHANNELS < 5
    float aMagSq = dot(a, a);
    float bMagSq = dot(b, b);
    return bMagSq > aMagSq ? b : a;
#else
    return (floatN)0;
#endif
}

floatN correctColorToLinear(floatN color)
{
    if (config.mipmapTextureUsage == MipmapTextureUsage_SRGB)
    {
        return SrgbToLinear(color);
    }
    else if (config.mipmapTextureUsage == MipmapTextureUsage_TangentSpaceNormal ||
             config.mipmapTextureUsage == MipmapTextureUsage_LocalSpaceNormal)
    {
        floatN outColor = mad(color, 2.0, -1.0); // Remap [0,1] → [-1,1]

#if NUM_CHANNELS >= 3
        if (config.mipmapTextureUsage == MipmapTextureUsage_TangentSpaceNormal)
        {
            outColor.z = sqrt(saturate(1.0 - dot(outColor.xy, outColor.xy)));
        }
#endif

        return outColor;
    }

    return color;
}

floatN correctColorForUsage(floatN color)
{
    if (config.mipmapTextureUsage == MipmapTextureUsage_TangentSpaceNormal ||
        config.mipmapTextureUsage == MipmapTextureUsage_LocalSpaceNormal)
    {
        return mad(normalize(color), 0.5, 0.5); // Remap [-1,1] → [0,1]
    }
    else if (config.mipmapTextureUsage == MipmapTextureUsage_SRGB)
    {
        return LinearToSrgb(color); // Assumes LinearToSrgb is overloaded
    }

    return color;
}

#endif

#endif // __GENERATEMIPMAPS_CS_HLSLI__