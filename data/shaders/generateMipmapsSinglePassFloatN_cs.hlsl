//
//
//  generateMipmapsSinglePassFloatN_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "include/generateMipmaps_cs.hlsli"

#define BASE_MIP_LEVEL 0U
#define PIX_PER_THREAD 4U
#define WG_SIZE        NUM_THREAD_X * NUM_THREAD_Y

// Used to store linear space color, and [-1, 1] scaled normal values
groupshared floatN intermediateResults[MAX_ARRAY_LAYERS][NUM_THREAD_Y][NUM_THREAD_X];
groupshared uint counter;

// Helper functions to load/store groupshared memory
void StoreToLocalMem(uint arraySlice, uint2 location, floatN value)
{
    intermediateResults[arraySlice][location.y][location.x] = value;
}

floatN LoadFromLocalMem(uint arraySlice, uint2 location)
{
    return intermediateResults[arraySlice][location.y][location.x];
}

floatN LoadFromTexture(uint arraySlice, uint mipLevel, uint2 location)
{
    return texture[mad(arraySlice, MAX_MIP_LEVELS, mipLevel)][location];
}

// Write color/normal value to texture
// Color - Convert linear to SRGB color space if necessary
// Normal - Scale to [0, 1]
void StoreToTexture(uint arraySlice, uint mipLevel, uint2 location, floatN value)
{
    floatN outPixel = correctColorForUsage(value);
    texture[mad(arraySlice, MAX_MIP_LEVELS, mipLevel)][location] = outPixel;
}

floatN ComputeOutPixel(floatN tl, floatN tr, floatN bl, floatN br)
{
    if (config.mipmapTextureUsage == MipmapTextureUsage_TangentSpaceNormal || config.mipmapTextureUsage == MipmapTextureUsage_LocalSpaceNormal)
    {
        return normalize(tl + tr + bl + br);
    }
    else
    {
        return 0.25 * (tr + tl + bl + br);
    }
}

// Load pixels from local memory
floatN ComputeOutPixelFromLocalMemory(uint arraySlice, uint2 tl, uint2 tr, uint2 bl, uint2 br)
{
    floatN tlPixel = LoadFromLocalMem(arraySlice, tl);
    floatN trPixel = LoadFromLocalMem(arraySlice, tr);
    floatN blPixel = LoadFromLocalMem(arraySlice, bl);
    floatN brPixel = LoadFromLocalMem(arraySlice, br);
    
    return ComputeOutPixel(tlPixel, trPixel, blPixel, brPixel);
}

// Load pixels from texture
// Color - Performs color space conversions
// Normals - Performance scaling from [0, 1] to [-1, 1]
floatN ComputeOutPixelFromTexture(uint arraySlice, uint mipLevel, uint2 tl, uint2 tr, uint2 bl, uint2 br)
{
    floatN tlPixel = correctColorToLinear(LoadFromTexture(arraySlice, mipLevel, tl));
    floatN trPixel = correctColorToLinear(LoadFromTexture(arraySlice, mipLevel, tr));
    floatN blPixel = correctColorToLinear(LoadFromTexture(arraySlice, mipLevel, bl));
    floatN brPixel = correctColorToLinear(LoadFromTexture(arraySlice, mipLevel, br));
    
    return ComputeOutPixel(tlPixel, trPixel, blPixel, brPixel);
}

void GenerateMipsIntermediate(
    in uint3 dispatchGroupID,       ///< Dispatch group ID. Pass uint3(0, 0, 0) if a single workgroup is present while generating mipmaps
    in uint3 dispatchGroupThreadID, ///< Dispatch group thread ID
    inout uint nextLevel,           ///< Start mip level
    inout uint remainingMipLevels)  ///< Remaining mip levels
{
    uint startLevel = nextLevel;
    uint endLevel = startLevel + min(remainingMipLevels, LOG2I(max(NUM_THREAD_X, NUM_THREAD_Y)));
    
    uint2 tl = dispatchGroupThreadID.xy * 2u;
    uint2 tr = tl + uint2(1, 0);
    uint2 bl = tl + uint2(0, 1);
    uint2 br = tl + uint2(1, 1);
    
    uint tmpRemainingMipLevels;
    
    for (uint arraySlice = 0; arraySlice < config.numLayers; arraySlice++)
    {
        tmpRemainingMipLevels = remainingMipLevels;
        
        for (uint level = startLevel; level < endLevel; ++level)
        {
            uint srcLevel = level - startLevel + 1;
            uint2 wgDstSize = uint2(NUM_THREAD_X >> srcLevel, NUM_THREAD_Y >> srcLevel);
        
            // Read from groupshared memory
            uint2 outLocation = dispatchGroupID.xy * wgDstSize.xy + dispatchGroupThreadID.xy;
            bool condition = (dispatchGroupThreadID.x < wgDstSize.x && dispatchGroupThreadID.y < wgDstSize.y);
        
            floatN outColor;
            if (condition)
            {
                outColor = ComputeOutPixelFromLocalMemory(arraySlice, tl, tr, bl, br);
            }

            GroupMemoryBarrierWithGroupSync();
        
            if (condition)
            {
                StoreToLocalMem(arraySlice, dispatchGroupThreadID.xy, outColor);
                StoreToTexture(arraySlice, level, outLocation, outColor);
            }
        
            tmpRemainingMipLevels--;
            GroupMemoryBarrierWithGroupSync();
        }
    }
    
    nextLevel = endLevel;
    remainingMipLevels = tmpRemainingMipLevels;
}

void GenerateMipsLevel_1_2(
    in uint3 dispatchThreadID,      ///< Dispatch thread ID
    in uint3 dispatchGroupThreadID, ///< Dispatch group thread ID
    out uint nextLevel,             ///< Next mip level to generate
    out uint remainingMipLevels)    ///< Remaining mip levels
{
    uint2 srcDim;
    uint endLevel;
    
    texture[BASE_MIP_LEVEL].GetDimensions(srcDim.x, srcDim.y);
    remainingMipLevels = LOG2I(max(srcDim.x, srcDim.y));
    nextLevel = 1;
    
    bool generateOnePixelPerThread = (srcDim.x < (NUM_THREAD_X * PIX_PER_THREAD)) && (srcDim.y < (NUM_THREAD_Y * PIX_PER_THREAD));
    
    uint tmpNextLevel;
    uint tmpRemainingMipLevels;
    
    if (generateOnePixelPerThread)
    {
        // Mip 1
        uint2 tl = dispatchThreadID.xy * 2;
        uint2 tr = tl + uint2(1, 0);
        uint2 bl = tl + uint2(0, 1);
        uint2 br = tl + uint2(1, 1);
        
        for (uint arraySlice = 0; arraySlice < config.numLayers; arraySlice++)
        {
            tmpNextLevel = nextLevel;
            tmpRemainingMipLevels = remainingMipLevels;
            
            floatN outColor = ComputeOutPixelFromTexture(arraySlice, BASE_MIP_LEVEL, tl, tr, bl, br);
            StoreToTexture(arraySlice, 1, dispatchThreadID.xy, outColor);

            tmpNextLevel++;
            tmpRemainingMipLevels--;

            if (tmpRemainingMipLevels)
            {
                // Initialize groupshared memory
                StoreToLocalMem(arraySlice, dispatchGroupThreadID.xy, outColor);
            }
        }
    }
    else
    {
        // Four pixels per work item - Mip 1
        // One pixel per work item - Mip 2
        floatN pix[4];
        uint stride = 2;

        for (uint arraySlice = 0; arraySlice < config.numLayers; arraySlice++)
        {
            tmpNextLevel = nextLevel;
            tmpRemainingMipLevels = remainingMipLevels;
            
            // Mip 1
            // Pix 1
            uint2 loc = dispatchThreadID.xy * stride;
            uint2 tl = loc * 2;
            uint2 tr = tl + uint2(1, 0);
            uint2 bl = tl + uint2(0, 1);
            uint2 br = tl + uint2(1, 1);
            
            pix[0] = ComputeOutPixelFromTexture(arraySlice, BASE_MIP_LEVEL, tl, tr, bl, br);
            StoreToTexture(arraySlice, 1, loc, pix[0]);
        
            // Pix 2
            loc = dispatchThreadID.xy * stride + uint2(1, 0);
            tl = loc * 2;
            tr = tl + uint2(1, 0);
            bl = tl + uint2(0, 1);
            br = tl + uint2(1, 1);

            pix[1] = ComputeOutPixelFromTexture(arraySlice, BASE_MIP_LEVEL, tl, tr, bl, br);
            StoreToTexture(arraySlice, 1, loc, pix[1]);

            // Pix 3
            loc = dispatchThreadID.xy * stride + uint2(0, 1);
            tl = loc * 2;
            tr = tl + uint2(1, 0);
            bl = tl + uint2(0, 1);
            br = tl + uint2(1, 1);

            pix[2] = ComputeOutPixelFromTexture(arraySlice, BASE_MIP_LEVEL, tl, tr, bl, br);
            StoreToTexture(arraySlice, 1, loc, pix[2]);

            // Pix 4
            loc = dispatchThreadID.xy * stride + uint2(1, 1);
            tl = loc * 2;
            tr = tl + uint2(1, 0);
            bl = tl + uint2(0, 1);
            br = tl + uint2(1, 1);

            pix[3] = ComputeOutPixelFromTexture(arraySlice, BASE_MIP_LEVEL, tl, tr, bl, br);
            StoreToTexture(arraySlice, 1, loc, pix[3]);

            tmpNextLevel++;
            tmpRemainingMipLevels--;
        
            // Mip 2
            floatN outColor;
            if (tmpRemainingMipLevels)
            {
                if (config.mipmapTextureUsage == MipmapTextureUsage_TangentSpaceNormal || config.mipmapTextureUsage == MipmapTextureUsage_LocalSpaceNormal)
                {
                    outColor = normalize(pix[0] + pix[1] + pix[2] + pix[3]);
                }
                else
                {
                    // Local memory is being used to store color in linear space, hence no SRGB <-> Linear space conversion needed
                    outColor = 0.25 * (pix[0] + pix[1] + pix[2] + pix[3]);
                }
                StoreToTexture(arraySlice, 2, dispatchThreadID.xy, outColor);

                tmpNextLevel++;
                tmpRemainingMipLevels--;
            }
        
            if (tmpRemainingMipLevels)
            {
                // Initialize groupshared memory
                StoreToLocalMem(arraySlice, dispatchGroupThreadID.xy, outColor);
            }
        }
    }
         
    nextLevel = tmpNextLevel;
    remainingMipLevels = tmpRemainingMipLevels;
    
    GroupMemoryBarrierWithGroupSync();
}

void GenerateMips_6_12(
    in uint3 dispatchGroupThreadID, ///< Dispatch group thread ID
    inout uint nextLevel,           ///< Next mip level to generate
    inout int remainingMipLevels)   ///< Remaining mip levels
{
    uint level = nextLevel;
    uint srcLevel = level - 1;
    uint2 dstDim;
    uint2 numPixels;
    texture[level].GetDimensions(dstDim.x, dstDim.y);
    floatN outColor;
    
    numPixels = uint2(1u << max(0, int(LOG2I(dstDim.x) - LOG2I(NUM_THREAD_X))),
                      1u << max(0, int(LOG2I(dstDim.y) - LOG2I(NUM_THREAD_Y))));

    uint2 start = dispatchGroupThreadID.xy * numPixels;
    uint2 end = start + numPixels;
    
    uint tmpNextLevel;
    uint tmpRemainingMipLevels;

    for (uint arraySlice = 0; arraySlice < config.numLayers; arraySlice++)
    {
        tmpNextLevel = nextLevel;
        tmpRemainingMipLevels = remainingMipLevels;
        
        for (uint y = start.y; y < end.y; ++y)
        {
            for (uint x = start.x; x < end.x; ++x)
            {
                uint2 outLocation = uint2(x, y);
        
                uint2 tl = outLocation.xy * 2;
                uint2 tr = tl + uint2(1, 0);
                uint2 bl = tl + uint2(0, 1);
                uint2 br = tl + uint2(1, 1);

                outColor = ComputeOutPixelFromTexture(arraySlice, srcLevel, tl, tr, bl, br);
                StoreToTexture(arraySlice, level, outLocation, outColor);
            }
        }

        tmpNextLevel++;
        tmpRemainingMipLevels--;
    
        if (tmpRemainingMipLevels <= 0)
        {
            continue;
        }
    
        if (numPixels.x == 1 && numPixels.y == 1)
        {
            StoreToLocalMem(arraySlice, dispatchGroupThreadID.xy, outColor);
        }
    }
    
    nextLevel = tmpNextLevel;
    remainingMipLevels = tmpRemainingMipLevels;
    
    GroupMemoryBarrierWithGroupSync();
}

[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void CSMain(
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uint3 dispatchGroupID : SV_GroupID,
    uint3 dispatchGroupThreadID : SV_GroupThreadID
)
{
    uint remainingMipLevels;
    uint nextLevel = 2;
    
    // Mip [1, 2]
    GenerateMipsLevel_1_2(dispatchThreadID, dispatchGroupThreadID, nextLevel, remainingMipLevels);
    if (!remainingMipLevels)
    {
        return;
    }
    
    // Mips [3, 5]
    GenerateMipsIntermediate(dispatchGroupID, dispatchGroupThreadID, nextLevel, remainingMipLevels);
    if (!remainingMipLevels)
    {
        return;
    }
    
    // Increment counter, exit all but a single workgroup
    if (dispatchGroupThreadID.x == 0 && dispatchGroupThreadID.y == 0)
    {
        InterlockedAdd(atomicCounter[0], 1, counter);
    }

    GroupMemoryBarrierWithGroupSync();

    if (counter != config.targetCounterValue)
    {
        return;
    }
    
    uint numIter = max(LOG2I(NUM_THREAD_X), LOG2I(NUM_THREAD_Y));
    do
    {
        // Mips [6, 12]
        GenerateMips_6_12(dispatchGroupThreadID, nextLevel, remainingMipLevels);
    } while (remainingMipLevels > numIter);
    if (!remainingMipLevels)
    {
        return;
    }
    
    // Mips [13, 15]
    GenerateMipsIntermediate(uint3(0, 0, 0), dispatchGroupThreadID, nextLevel, remainingMipLevels);
}
