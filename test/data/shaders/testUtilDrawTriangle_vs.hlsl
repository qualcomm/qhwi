//
//
//  testUtilDrawTriangle_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    uint vertexID   : SV_VertexID;
};

struct VSOutput
{
    float4 pos      : SV_POSITION;
    float4 color    : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    const float2 Positions[] =
    {
        float2(0.0f, 0.0f),
        float2(1.0f, 1.0f),
        float2(1.0f, 0.0f)
    };
    VSOutput result;
    result.pos = float4(Positions[input.vertexID], 0.5f, 1.0f);
    result.color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    return result;
}
