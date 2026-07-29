//
//
//  testSpecializationConstants.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSInput
{
    float2 pos : POSITION0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
};

[[vk::constant_id(0)]] const int s_Color = 0;

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = float4(input.pos, 0.5f, 1.0f);
    return result;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    switch (s_Color)
    {
        case 1: return float4(1.0f, 0.0f, 0.0f, 1.0f);
        default: return float4(0.0f, 1.0f, 0.0f, 1.0f);
    }
}
