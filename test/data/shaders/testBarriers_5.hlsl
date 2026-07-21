//
//
//  testBarriers.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct DrawSampleTask0_VS_Input
{
    float2 pos : POSITION0;
    float2 uv : TEXCOORD0;
};

struct DrawSampleTask0_VS_Output
{
    float4 pos: SV_POSITION;
    float2 uv : TEXCOORD0;
};

DrawSampleTask0_VS_Output DrawSampleTask0_VS_Main(DrawSampleTask0_VS_Input input)
{
    DrawSampleTask0_VS_Output output;
    output.pos = float4(input.pos, 0.5f, 1.0f);
    output.uv = input.uv;
    return output;
}

Texture2D DrawSampleTask0_PS_texture : register(t0);
SamplerState DrawSampleTask0_PS_sampler : register(s0);

float4 DrawSampleTask0_PS_Main(DrawSampleTask0_VS_Output input) : SV_TARGET
{
    return DrawSampleTask0_PS_texture.Sample(DrawSampleTask0_PS_sampler, input.uv);
}
