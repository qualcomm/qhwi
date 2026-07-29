//
//
//  testMultipleRenderTargets2_ps.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uvw : TEXCOORD0;
};

Texture2D texture0 : register(t0);
Texture2D texture1 : register(t1);
Texture2D texture2 : register(t2);
SamplerState sampler0 : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    return float4(texture0.Sample(sampler0, input.uvw).r, texture1.Sample(sampler0, input.uvw).r, texture2.Sample(sampler0, input.uvw).r, 1.0);
}

