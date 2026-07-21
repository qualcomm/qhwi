//
//
//  vk_custom_resolve.hlsl
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
    // Centered triangle occupying the middle half of the screen
    const float2 Pos[] = { float2(0.0, -0.5), float2(-0.5, 0.5), float2(0.5, 0.5) };
    const float2 UV[]  = { float2(0.5, 0.0),  float2(0.0, 1.0),  float2(1.0, 1.0) };
    result.pos = float4(Pos[VertexId], 0.0f, 1.0f);
    result.uv  = UV[VertexId];
    return result;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // Render a simple gradient pattern to demonstrate MSAA custom resolve
    float2 uv = input.uv;
    float r = uv.x;
    float g = uv.y;
    float b = 1.0f - (uv.x + uv.y) * 0.5f;
    return float4(r, g, b, 1.0f);
}