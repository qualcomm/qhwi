//
//
//  testMultiView.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos : SV_POSITION;
};

VSOutput VSMain(uint VertexId : SV_VertexID, uint ViewId : SV_ViewID)
{
    VSOutput result;
    float X[] = { .1, .8 };
    float Y[] = { .7, .2 };
    float x = X[ViewId], y = Y[ViewId];
    const float2 Pos[] = { float2(-x, -y), float2(-x, y), float2(x, -y), float2(x, y) };
    float2 pos = Pos[VertexId];
    result.pos = float4(pos, 0.0f, 1.0f);
    return result;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
