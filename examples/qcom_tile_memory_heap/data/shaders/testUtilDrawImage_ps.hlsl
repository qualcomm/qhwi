//
//
// testUtilDrawImage_ps.hlsl
// QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

struct PushConstants
{
    uint samplePixelCenter;
    float2 frameBufferDimensions;
    uint pad;    
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants : register(b0);

Texture2D texture0    : register(t0);
SamplerState sampler0 : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    float2 samplePosition;
    if(pushConstants.samplePixelCenter != 0)
    {
        // Keep the sample position as the center of the pixel
        samplePosition = input.uv;
    }
    else
    {
        // Change the sample point to be the top left of the pixel
        samplePosition = floor(input.pos) / pushConstants.frameBufferDimensions;
    }

    return texture0.Sample(sampler0, samplePosition);
}
