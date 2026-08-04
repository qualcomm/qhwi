//
//
//  testMultipleRenderTargets1_ps.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 color : TEXCOORD0;
};

struct PSOutput
{
    float redComp : SV_Target0;
    float greenComp : SV_Target1;
    float blueComp : SV_Target2;
};

PSOutput PSMain(VSOutput input)
{
    PSOutput ret_comp;
    ret_comp.redComp = input.color.r;
    ret_comp.greenComp = input.color.g;
    ret_comp.blueComp = input.color.b;
    return ret_comp;
}

