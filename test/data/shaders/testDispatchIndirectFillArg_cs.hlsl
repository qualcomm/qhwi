//
//
//  testDispatchIndirectFillArg_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

struct DispatchArg
{
    uint numThreadGroupX;
    uint numThreadGroupY;
    uint numThreadGroupZ;
};

RWStructuredBuffer<DispatchArg> indirectDispatchArg : register(u0);

[numThreads(1, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
    DispatchArg arg = { 4, 1, 1 };

    indirectDispatchArg[dispatchThreadID] = arg;
}
