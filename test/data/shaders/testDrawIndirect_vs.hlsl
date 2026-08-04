//
//
//  testDrawIndirect_vs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos     : POSITION;
    float4 col     : COLOR;
    float2 instPos : INSTANCEPOSITION;
    float4 instCol : INSTANCECOLOR;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = float4(input.pos + input.instPos, 0.5, 1.0f);
    result.col = input.instCol;
    return result;
}
