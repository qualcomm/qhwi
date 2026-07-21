//
// testRayPipeline.hlsli
// QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#define FLT_MIN 1.175494351e-38F
#define FLT_MAX 3.402823466e+38F

RaytracingAccelerationStructure accelerationStructure : register(t0);
RWTexture2D<float4> outputTexture : register(u0);

struct Payload
{
    float4 color;
};

struct Attributes
{
    float2 barycentrics;
};

#if SPIRV
// Workaround for SPIRV validation error faceforward lib_6_4 shader profile
#    define ENABLE_RAYGEN_SHADER RAYGEN_SHADER
#    define ENABLE_MISS_SHADER   MISS_SHADER
#    define ENABLE_HIT_SHADER    HIT_SHADER
#else
#    define ENABLE_RAYGEN_SHADER 1
#    define ENABLE_MISS_SHADER   1
#    define ENABLE_HIT_SHADER    1
#endif

#ifdef ENABLE_RAYGEN_SHADER
[shader("raygeneration")] void RayGeneration_Main()
{
    uint3 dispatchIndex = DispatchRaysIndex();
    uint3 dispatchDimension = DispatchRaysDimensions();
    float3 origin = dispatchIndex.xyz;

    RayDesc rayDesc;
    rayDesc.Origin = origin;
    rayDesc.TMin = FLT_MIN;
    rayDesc.Direction = float3(0.0, 0.0, 1.0);
    rayDesc.TMax = FLT_MAX;

    Payload payload;

    TraceRay(accelerationStructure, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);

    outputTexture[dispatchIndex.xy] = payload.color;
}
#endif // ENABLE_RAYGEN_SHADER

#if ENABLE_MISS_SHADER
[shader("miss")]
void Miss_Main(inout Payload payload)
{
    payload.color = float4(1.0, 1.0, 0.0, 1.0f);
}
#endif // ENABLE_MISS_SHADER
