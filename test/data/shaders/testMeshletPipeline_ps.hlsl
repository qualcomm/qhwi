//
//
//  testMeshletPipeline_ps.hlsl
//  QHWI
//
//  Copyright (C) 2023 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct MSOutput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};

float4 PSMain(MSOutput input) : SV_TARGET
{
    return input.col;
}
