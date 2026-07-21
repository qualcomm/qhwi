//
// testRayPipeline.hlsl
// QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include"testRayPipeline.hlsli"

#if ENABLE_HIT_SHADER
[shader("closesthit")]
void ClosestHit_Main(inout Payload payload, in Attributes attributes)
{
    payload.color = float4(0.0, 0.0, 1.0, 1.0f); 
}
#endif // ENABLE_HIT_SHADER
