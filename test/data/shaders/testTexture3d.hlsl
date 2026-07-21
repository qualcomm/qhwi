//
//
//  testTexture3d.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
struct VSInput
{
    float3 pos : POSITION0;
    float3 uvw : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 uvw : TEXCOORD0;
};

struct CBuffer
{
    float4x4 mat;
};

ConstantBuffer<CBuffer> transform : register(b0);
Texture3D texture0 : register(t0);
SamplerState sampler0 : register(s0);

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = mul(transform.mat, float4(input.pos, 1.0));
    result.uvw = input.uvw;
    return result;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return texture0.Sample(sampler0, input.uvw);
}
