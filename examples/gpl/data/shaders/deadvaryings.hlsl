//
//
//  deadVaryings.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

// This shader tests the effect of gpl and the shader compiler with a large number
// of unused (dead) varyings.
struct VSInput
{
    float4 pos : POSITION0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 color   : COLOR0;
    float4 unused0 : COLOR1;
    float4 unused1 : COLOR2;
    float4 unused2 : COLOR3;
    float4 unused3 : COLOR4;
    float4 unused4 : COLOR5;
    float4 unused5 : COLOR6;
    float4 unused6 : COLOR7;
    float4 unused7 : COLOR8;
    float4 unused8 : COLOR9;
    float4 unused9 : COLOR10;
};

struct CBInput
{
    float4x4 modelViewProj;
};

[[vk::push_constant]] ConstantBuffer<CBInput> cbIn : register(b0);

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = mul(cbIn.modelViewProj, input.pos);
    result.color = input.color;
    result.unused0 = input.color * 0.0;
    result.unused1 = input.color * 0.1;
    result.unused2 = input.color * 0.2;
    result.unused3 = input.color * 0.3;
    result.unused4 = input.color * 0.4;
    result.unused5 = input.color * 0.5;
    result.unused6 = input.color * 0.6;
    result.unused7 = input.color * 0.7;
    result.unused8 = input.color * 0.8;
    result.unused9 = input.color * 0.9;
    return result;
}

float4 PSMain(VSOutput input)
    : SV_TARGET
{
    return input.color;
}
