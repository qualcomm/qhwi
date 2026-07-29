//
//
//  testRenderToCubemap_ps.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos      : SV_POSITION;
};

struct PSOutput
{
    float4 rtv0 : SV_TARGET0;
    float4 rtv1 : SV_TARGET1;
    float4 rtv2 : SV_TARGET2;
    float4 rtv3 : SV_TARGET3;
    float4 rtv4 : SV_TARGET4;
    float4 rtv5 : SV_TARGET5;
};

PSOutput PSMain(VSOutput input)
{
    PSOutput output;

    output.rtv0 = float4(1.0, 0.0, 0.0, 1.0);
    output.rtv1 = float4(0.0, 1.0, 0.0, 1.0);
    output.rtv2 = float4(0.0, 0.0, 1.0, 1.0);
    output.rtv3 = float4(0.0, 1.0, 1.0, 1.0);
    output.rtv4 = float4(1.0, 0.0, 1.0, 1.0);
    output.rtv5 = float4(1.0, 1.0, 0.0, 1.0);
    return output;
}
