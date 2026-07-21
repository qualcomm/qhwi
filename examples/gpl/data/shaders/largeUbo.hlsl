//
//
// largeUbo.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//


// This shader tests the effect of gpl and the shader compiler with a large
// uniform buffer object.
struct VSInput
{
    float4 pos : POSITION0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
};

struct CBInput
{
    float4x4 modelViewProj;
};

[[vk::push_constant]] ConstantBuffer<CBInput> cbIn : register(b0);

#define ARRAY_SIZE 512
cbuffer ubo : register(b1)
{
    float4 values[ARRAY_SIZE];
}

VSOutput VSMain(VSInput input)
{
    VSOutput result;
    result.pos = mul(cbIn.modelViewProj, input.pos);
    result.color = input.color;
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
        result.color += values[i];
    }

    return result;
}

float4 PSMain(VSOutput input)
    : SV_TARGET
{
    return input.color;
}
