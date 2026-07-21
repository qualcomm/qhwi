//
//
//  tonemap.hlsl
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(uint VertexId : SV_VertexID)
{
    VSOutput result;
    // Full-screen triangle strip quad
    const float2 Pos[] = { float2(-1, -1), float2(-1, 1), float2(1, -1), float2(1, 1) };
    const float2 UV[]  = { float2(0, 1),   float2(0, 0),  float2(1, 1),  float2(1, 0) };
    result.pos = float4(Pos[VertexId], 0.0f, 1.0f);
    result.uv  = UV[VertexId];
    return result;
}

Texture2D    resolvedColor : register(t0);
SamplerState resolvedSampler : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    // Sample the resolved texture and convert to grayscale
    // using standard luminance weights (ITU-R BT.709).
    float4 color = resolvedColor.Sample(resolvedSampler, input.uv);
    float luminance = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    return float4(luminance, luminance, luminance, color.a);
}
