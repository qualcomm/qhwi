//
//
//  blit_cs.hlsl
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

# define NUM_THREAD_X   8
# define NUM_THREAD_Y   8

RWTexture2D<float4> src : register(u0);
RWTexture2D<float4> dst : register(u1);

[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void CSMain(
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uint3 dispatchGroupID : SV_GroupID,
    uint3 dispatchGroupThreadID : SV_GroupThreadID
)
{
    dst[dispatchThreadID.xy] = src[dispatchThreadID.xy];
}
