//
//
//  testDrawIndirect_ps.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct VSOutput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};

float4 PSMain(VSOutput input) : SV_TARGET
{
    return input.col;
}
