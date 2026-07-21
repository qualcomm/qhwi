//
//
//  drawImage.hlsl
//
//  Copyright (C) 2023-2024 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos : POSITION0;
    float2 uv  : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = float4(input.pos, 0.5f, 1.0f);
    result.uv = input.uv;
    return result;
}

Texture2D texture0      : register(t0);
SamplerState sampler0   : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    return texture0.Sample(sampler0, input.uv);
}
