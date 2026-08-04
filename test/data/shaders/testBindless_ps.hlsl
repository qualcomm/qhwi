//
//
//  testBindless_ps.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#ifdef METAL
#define MAX_TEXTURES 100
#else
#define MAX_TEXTURES
#endif

Texture2D textures[MAX_TEXTURES] : register(t0, space1);
SamplerState sampler0 : register(s0);

struct VSOutput
{
    float4 pos       : SV_POSITION;
    int textureIndex : TEXINDEX0;
};

float4 PSMain(VSOutput input) : SV_TARGET
{
    return textures[NonUniformResourceIndex(input.textureIndex)].Sample(sampler0, float2(0.5f, 0.5f));
}
