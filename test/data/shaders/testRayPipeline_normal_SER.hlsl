//
// testRayPipeline_normal_SER.hlsl
// QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
// SER-enabled shader for both DirectX 12 and Vulkan

#define SPIRV (defined(USE_SER))

#include "serHelper.hlsli"

#define FLT_MIN       1.175494351e-38F
#define FLT_MAX       3.402823466e+38F
#define MAX_RAY_DEPTH 5

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

RaytracingAccelerationStructure     accelerationStructure   : register(t0);
RWTexture2D<float4>                 outputTexture           : register(u0);
RWStructuredBuffer<float4>          sphereColor             : register(u1);

struct[raypayload] PayloadDepth
{
    float4 color : read(miss, caller) : write(closesthit, miss, caller);
    uint rayDepth : read(closesthit, miss, caller) : write(closesthit, caller);
};

struct Attributes
{
    float2 barycentrics;
};

#if SPIRV
[[vk::ext_storage_class(RayPayloadKHR)]] static PayloadDepth payloadGlobal;
#endif

#if SPIRV
[[vk::ext_instruction(OpHitObjectRecordEmptyNV)]]
void hitObjectRecordEmptyNV([[vk::ext_reference]] HitObjectNV hit);

[[vk::ext_instruction(OpHitObjectTraceRayNV)]]
void hitObjectTraceRayNV([[vk::ext_reference]] HitObjectNV hit,
                         RaytracingAccelerationStructure as,
                         uint RayFlags,
                         uint CullMask,
                         uint SBTOffset,
                         uint SBTStride,
                         uint MissIndex,
                         float3 RayOrigin,
                         float RayTmin,
                         float3 RayDirection,
                         float RayTMax,
                         [[vk::ext_reference]] [[vk::ext_storage_class(RayPayloadKHR)]] PayloadDepth payload);

[[vk::ext_instruction(OpReorderThreadWithHitObjectNV)]]
void reorderThreadWithHitObjectNV([[vk::ext_reference]] HitObjectNV hit);

[[vk::ext_instruction(OpHitObjectExecuteShaderNV)]]
void hitObjectExecuteShaderNV([[vk::ext_reference]] HitObjectNV hit, [[vk::ext_reference]] [[vk::ext_storage_class(RayPayloadKHR)]] PayloadDepth payload);

[[vk::ext_instruction(OpHitObjectIsHitNV)]]
bool hitObjectIsHitNV([[vk::ext_reference]] HitObjectNV hit);
#endif

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

#if SPIRV
    createHitObjectNV();
    payloadGlobal.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    payloadGlobal.rayDepth = 0;

    HitObjectNV hit;
    hitObjectRecordEmptyNV(hit);
    hitObjectTraceRayNV(hit, accelerationStructure, 0x0, 0xff, 0, 0, 0, rayDesc.Origin, rayDesc.TMin, rayDesc.Direction, rayDesc.TMax, payloadGlobal);
    reorderThreadWithHitObjectNV(hit);
    hitObjectExecuteShaderNV(hit, payloadGlobal);
    outputTexture[dispatchIndex.xy] = payloadGlobal.color;
#else
    PayloadDepth payload;
    payload.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    payload.rayDepth = 0;
    dx::HitObject hit = dx::HitObject::TraceRay(accelerationStructure, RAY_FLAG_NONE, 0xff, 0, 0, 0, rayDesc, payload);
    dx::MaybeReorderThread(hit);
    dx::HitObject::Invoke(hit, payload);
    outputTexture[dispatchIndex.xy] = payload.color;
#endif
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

#if SPIRV
    if (payloadGlobal.rayDepth < 1)
    {
        payloadGlobal.color = float4(0.25, 0.25, 0.25, 1.0f);
    }
#endif
}
#endif // ENABLE_MISS_SHADER

#if ENABLE_HIT_SHADER
[shader("closesthit")]
void ClosestHit_Normal(inout PayloadDepth payload, in Attributes attributes)
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

#if SPIRV
    payloadGlobal.color = float4((normal + 1) * 0.5, 1.0f);
#endif
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

#if SPIRV
        payloadGlobal.color.xyz = float3(depthFactor, depthFactor, depthFactor);
#endif
    }
    else
    {
        // Base case for recursion: set a default color
        payload.color.xyz = float3(0.1f, 0.1f, 0.1f);
#if SPIRV
        payloadGlobal.color.xyz = float3(0.1f, 0.1f, 0.1f);
#endif
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

#if SPIRV
    payloadGlobal.color = float4(color.rgb, 1.0f);
#endif
}
#endif // ENABLE_HIT_SHADER
