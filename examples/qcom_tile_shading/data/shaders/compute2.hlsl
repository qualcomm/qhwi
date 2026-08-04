//
//
//  compute2.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct SSBOOutS { float4 DataOut; };

[[vk::binding(0)]]
RWStructuredBuffer<SSBOOutS> SSBOOut;

struct TransientOutS
{
    float4 dataTransient;
};

[[vk::tile_attachment_qcom]]
[[vk::binding(2)]]
uniform RWStructuredBuffer<TransientOutS> TransientOut;

[numthreads(1, 1, 1)]
void main()
{
    SSBOOut[0].DataOut = TransientOut[0].dataTransient;
}