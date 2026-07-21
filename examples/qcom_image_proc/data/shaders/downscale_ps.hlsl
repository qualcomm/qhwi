//
//
//  downscale_ps.hlsl
//
//  Copyright (C) 2021-2024 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos      : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

Texture2D inputTex          : register(t0);
SamplerState inputSampler   : register(s0);
Texture2DArray kernelTex    : register(t1);
SamplerState kernelSampler  : register(s1);

float4 PSMain(VSOutput input) : SV_TARGET
{
    const float2 halfTexel = float2(0.5, 0.5);

    // TODO: Pass all this in 
    uint kWidth = 0;
    uint kHeight = 0;
    uint nLayers = 0;
    kernelTex.GetDimensions(kWidth, kHeight, nLayers);
    float2 kDim = float2(kWidth, kHeight);
    float2 kScale = 1.0f / kDim;
    uint iWidth = 0;
    uint iHeight = 0;
    inputTex.GetDimensions(iWidth, iHeight);
    float2 iScale = 1.0f / float2(iWidth, iHeight);

    float2 uv = input.uv;                      // This is at the center of the output texel
    uv -= (0.5 * kDim - halfTexel) * iScale;   // move starting UV to upper left corner of kernel

    float3 outColor = float3(0, 0, 0);
    float2 uvOffset;
    for (int j = 0; j < kHeight; ++j)
    {
        uvOffset.y = float(j);
        for (int i = 0; i < kWidth; ++i)
        {
            uvOffset.x = float(i);
            float2 uvOffset = float2(float(i), float(j));
            outColor += inputTex.Sample(inputSampler, uv + uvOffset * iScale).rgb
                      * kernelTex.Sample(kernelSampler, float3((uvOffset + halfTexel) * kScale, 0)).r;
        }
    }

    return float4(outColor, 1.0);
}
