//
//
//  vert2.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

static float4 gl_Position;
static float4 position;
static float2 texcoord;
static float2 in_texcoord;

struct SPIRV_Cross_Input
{
    float4 position : TEXCOORD0;
    float2 in_texcoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float2 texcoord : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    gl_Position = position;
    texcoord = in_texcoord;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    position = stage_input.position;
    in_texcoord = stage_input.in_texcoord;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.texcoord = texcoord;
    return stage_output;
}
