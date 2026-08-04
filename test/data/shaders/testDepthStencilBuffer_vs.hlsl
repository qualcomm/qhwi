//
//
//  testDepthStencilBuffer_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float3 pos : POSITION;
    float3 col : COLOR;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = float4(input.pos, 1.0f);
    result.col = float4(input.col, 1.0f);
    return result;
}
