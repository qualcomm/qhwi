//
//
//  testRayTracing_cs.hlsl
//  QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#define FLT_MIN 1.175494351e-38F
#define FLT_MAX 3.402823466e+38F

RaytracingAccelerationStructure accelerationStructure : register(t0);
RWTexture2D<float4>             outputTexture         : register(u0);

[numThreads(16, 4, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RayQuery<RAY_FLAG_NONE> q;
    RayDesc                 ray;

    ray.Origin    = float3(dispatchThreadID.x, dispatchThreadID.y, 0.0f);
    ray.Direction = float3(0.0f, 0.0f, 1.0f);
    ray.TMin      = FLT_MIN;
    ray.TMax      = FLT_MAX;

    q.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, 0xff, ray);

    q.Proceed();

    float4 rgbaOut;

    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        float2 barycentrics = q.CommittedTriangleBarycentrics();

        rgbaOut = float4(0.0f, 1.0f, 0.0f, 1.0f);
    }
    else
    {
        rgbaOut = float4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    outputTexture[dispatchThreadID.xy] = rgbaOut;
}
 
