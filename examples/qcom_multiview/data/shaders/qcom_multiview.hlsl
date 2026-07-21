//
//
//  qcom_multiview.hlsl
//  QHWI
//
//  Copyright (C) 2024 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear

struct VSOutput
{
    float4 pos : SV_POSITION;
    uint viewId : TEXCOORD0;
};

VSOutput VSMain(uint VertexId : SV_VertexID, uint ViewId : SV_ViewID)
{
    VSOutput result;
    const float2 Pos[] = { float2(-1, -1), float2(-1, 1), float2(1, -1), float2(1, 1) };
    float2 pos = Pos[VertexId];
    result.pos = float4(pos, 0.0f, 1.0f);
    result.viewId = ViewId;
    return result;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    if (input.viewId == 0)
    {
        return float4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        return float4(0.0f, 1.0f, 0.0f, 1.0f);
    }
}
