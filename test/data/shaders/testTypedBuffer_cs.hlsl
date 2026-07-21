//
//
//  testTypedBuffer_cs.hlsl
//  QHWI
//
//  Copyright (C) 2024 Qualcomm Technologies, Inc.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//

Buffer<uint4> inBuffer  : register(t0);
RWStructuredBuffer<uint4> outBuffer : register(u0);

[numThreads(1, 1, 1)]
void CSMain(uint dispatchThreadID : SV_DispatchThreadID)
{
    for (uint32_t j = 0; j < 16; j++)
    {
        outBuffer[j] = inBuffer.Load(j);
    }
}
