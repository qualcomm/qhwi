//
//
//  frag1.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

Texture2D<float4> tex : register(t0);
SamplerState tex_sampler : register(s2);

static float4 oColour;
static float2 texcoord;

struct SPIRV_Cross_Input
{
    float2 texcoord : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 oColour : SV_Target0;
};

void frag_main()
{
    oColour = tex.SampleBias(tex_sampler, texcoord, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    texcoord = stage_input.texcoord;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.oColour = oColour;
    return stage_output;
}
