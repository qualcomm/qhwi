//
// testRayPipeline_normal.hlsl
// QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include"testRayPipeline.hlsli"
#define MAX_RAY_DEPTH 5

RWStructuredBuffer<float4>      sphereColor           : register(u1);

struct PayloadDepth
{
    float4 color;
    uint rayDepth;
};

#ifdef ENABLE_RAYGEN_SHADER
[shader("raygeneration")]
void RayGeneration_Depth()
{
    uint3 dispatchIndex = DispatchRaysIndex();
    uint3 dispatchDimension = DispatchRaysDimensions();
    float3 origin = dispatchIndex.xyz;

    RayDesc rayDesc;
    rayDesc.Origin = origin;
    rayDesc.TMin = FLT_MIN;
    rayDesc.Direction = float3(0.0, 0.0, 1.0);
    rayDesc.TMax = FLT_MAX;

    PayloadDepth payload;
    payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    payload.rayDepth = 0;

    TraceRay(accelerationStructure, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);

    outputTexture[dispatchIndex.xy] = payload.color;
}
#endif // ENABLE_RAYGEN_SHADER

#if ENABLE_MISS_SHADER
[shader("miss")]
void Miss_Depth(inout PayloadDepth payload)
{
    if (payload.rayDepth < 1)
    {
        payload.color = float4(0.25, 0.25, 0.25, 1.0f);
    }
}
#endif // ENABLE_MISS_SHADER

#if ENABLE_HIT_SHADER
[shader("closesthit")] void ClosestHit_Normal(inout PayloadDepth payload, in Attributes attributes)
{
    // Calculate the hit point
    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    uint instanceID = InstanceID();

    // Get the transformation matrix for the instance
    float3x4 instanceTransform = ObjectToWorld3x4();

    float scale = instanceTransform._m00;
    float3 sphereCenter = float3(instanceTransform._m03,  // Translation X
                                 instanceTransform._m13,  // Translation Y
                                 instanceTransform._m23); // Translation Z

    // Calculate the normal
    float3 normal = normalize((sphereCenter - hitPoint) / scale);

    // Color the sphere based on the normal
    payload.color = float4((normal + 1) * 0.5, 1.0f);
}
#endif // ENABLE_HIT_SHADER

#if ENABLE_HIT_SHADER
[shader("closesthit")]
void ClosestHit_RayDepth(inout PayloadDepth payload, in Attributes attributes)
{
    if (payload.rayDepth < MAX_RAY_DEPTH)
    {
        float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        float3x4 instanceTransform = ObjectToWorld3x4();
        float scale = instanceTransform._m00;
        float3 sphereCenter = float3(instanceTransform._m03, instanceTransform._m13, instanceTransform._m23);

        float3 normal = normalize((hitPoint - sphereCenter) / scale);
        // Calculate the reflection direction
        float3 incomingDir = normalize(WorldRayDirection());
        float3 reflectDir = reflect(incomingDir, normal);

        // Trace secondary ray for reflection
        RayDesc secondaryRay;
        secondaryRay.Origin = hitPoint;
        secondaryRay.TMin = 0.001f; // Avoid self-intersection
        secondaryRay.Direction = reflectDir;
        secondaryRay.TMax = FLT_MAX;

        payload.rayDepth += 1; // Increment ray depth
        TraceRay(accelerationStructure, RAY_FLAG_NONE, 0xff, 0, 0, 0, secondaryRay, payload);

        // Visualize depth by mapping depth to color
        float depthFactor = payload.rayDepth * 0.10f; // Normalize depth
                                                      // Accumulate color from secondary ray
        payload.color.xyz = float3(depthFactor, depthFactor, depthFactor);
    }
    else
    {
        // Base case for recursion: set a default color
        payload.color.xyz = float3(0.1f, 0.1f, 0.1f);
    }
}
#endif // ENABLE_HIT_SHADER

#if ENABLE_HIT_SHADER
[shader("closesthit")]
void ClosestHit_Color(inout PayloadDepth payload, in Attributes attributes)
{
    float3 hitPoint = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    uint instanceID = InstanceID();
    float4 color = sphereColor[instanceID];

    payload.color = float4(color.r, color.g, color.b, 1.0f);
}
#endif // ENABLE_HIT_SHADER
