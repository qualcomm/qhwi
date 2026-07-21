//
//
//  generateMipmapsMultiPassFloatN_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include "include/generateMipmaps_cs.hlsli"

floatN ComputeOutputPixelNearest(uint srcMipLevel, uint2 dstLocation)
{
    return texture[srcMipLevel][computeSourceLocationFromDestination(dstLocation)];
}

floatN ComputeOutputPixelLinear(uint srcMipLevel, uint2 dstLocation)
{
    floatN accumulatedColor = (floatN)0;
    uint sampleCount = 0;

    int2 baseCoord = computeSourceLocationFromDestinationTopLeft(dstLocation);

    int minY = max(baseCoord.y, 0);
    int minX = max(baseCoord.x, 0);
    int maxY = min(baseCoord.y + config.kernelSize.y, config.srcSize.y);
    int maxX = min(baseCoord.x + config.kernelSize.x, config.srcSize.x);

    for (uint y = minY; y < maxY; ++y)
    {
        for (uint x = minX; x < maxX; ++x)
        {
            floatN color = correctColorToLinear(texture[srcMipLevel][uint2(x, y)]);
            accumulatedColor += color;
            sampleCount++;
        }
    }

    accumulatedColor /= float(sampleCount);
    return correctColorForUsage(accumulatedColor);
}

floatN ComputeOutputPixelMin(uint srcMipLevel, uint2 dstLocation)
{
    int2 baseCoord = computeSourceLocationFromDestinationTopLeft(dstLocation);
    floatN currMinColor = MAX_VAL;

    int minY = max(baseCoord.y, 0);
    int minX = max(baseCoord.x, 0);
    int maxY = min(baseCoord.y + config.kernelSize.y, config.srcSize.y);
    int maxX = min(baseCoord.x + config.kernelSize.x, config.srcSize.x);

    for (uint y = minY; y < maxY; ++y)
    {
        for (uint x = minX; x < maxX; ++x)
        {
            currMinColor = minColor(currMinColor, correctColorToLinear(texture[srcMipLevel][uint2(x, y)]));
        }
    }
    return correctColorForUsage(currMinColor);
}

floatN ComputeOutputPixelMax(uint srcMipLevel, uint2 dstLocation)
{
    int2 baseCoord = computeSourceLocationFromDestinationTopLeft(dstLocation);
    floatN currMaxColor = (floatN)0;

    int minY = max(baseCoord.y, 0);
    int minX = max(baseCoord.x, 0);
    int maxY = min(baseCoord.y + config.kernelSize.y, config.srcSize.y);
    int maxX = min(baseCoord.x + config.kernelSize.x, config.srcSize.x);

    for (uint y = minY; y < maxY; ++y)
    {
        for (uint x = minX; x < maxX; ++x)
        {
            currMaxColor = maxColor(currMaxColor, correctColorToLinear(texture[srcMipLevel][uint2(x, y)]));
        }
    }
    return correctColorForUsage(currMaxColor);
}

floatN ComputeOutputPixel(uint srcMipLevel, uint2 dstLocation)
{
    if (config.mipmapFilter == MipmapFilter_Nearest)
    {
        return ComputeOutputPixelNearest(srcMipLevel, dstLocation);
    }
    else if(config.mipmapFilter == MipmapFilter_Linear)
    {
        return ComputeOutputPixelLinear(srcMipLevel, dstLocation);
    }
    else if (config.mipmapFilter == MipmapFilter_Min)
    {
        return ComputeOutputPixelMin(srcMipLevel, dstLocation);
    }
    if (config.mipmapFilter == MipmapFilter_Max)
    {
        return ComputeOutputPixelMax(srcMipLevel, dstLocation);
    }
    else
    {
        return (floatN)0;
    }
}


[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void CSMain(
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uint3 dispatchGroupID : SV_GroupID,
    uint3 dispatchGroupThreadID : SV_GroupThreadID
)
{
    if (dispatchThreadID.x >= config.dstSize.x || dispatchThreadID.y >= config.dstSize.y)
    {
        return;
    }

    for (uint arrayLayer = 0; arrayLayer < config.numLayers; arrayLayer++)
    {
        uint dstTextureIndex = arrayLayer * MAX_MIP_LEVELS + config.dstMipLevel;
        texture[dstTextureIndex][dispatchThreadID.xy] = ComputeOutputPixel(dstTextureIndex - 1, dispatchThreadID.xy);
    }
}
