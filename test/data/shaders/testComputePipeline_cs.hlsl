//
//
//  testComputePipeline_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

RWByteAddressBuffer globalCounter : register(u0);

[numThreads(16, 4, 2)]
void CSMain()
{
    uint oldValue = 0;
    globalCounter.InterlockedAdd(0, 1, oldValue);
}
