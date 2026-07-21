//
// serHelper.hlsli
// QHWI
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
// Helper for SER Compilation
// Ref : https://docs.vulkan.org/refpages/latest/refpages/source/VK_NV_ray_tracing_invocation_reorder.html

#ifdef SPIRV
#define ShaderInvocationReorderNV 5383
#define HitObjectAttributeNV 5385

#define OpHitObjectRecordHitMotionNV 5249
#define OpHitObjectRecordHitWithIndexMotionNV 5250
#define OpHitObjectRecordMissMotionNV 5251
#define OpHitObjectGetWorldToObjectNV 5252
#define OpHitObjectGetObjectToWorldNV 5253
#define OpHitObjectGetObjectRayDirectionNV 5254
#define OpHitObjectGetObjectRayOriginNV 5255
#define OpHitObjectTraceRayMotionNV 5256
#define OpHitObjectGetShaderRecordBufferHandleNV 5257
#define OpHitObjectGetShaderBindingTableRecordIndexNV 5258
#define OpHitObjectRecordEmptyNV 5259
#define OpHitObjectTraceRayNV 5260
#define OpHitObjectRecordHitNV 5261
#define OpHitObjectRecordHitWithIndexNV 5262
#define OpHitObjectRecordMissNV 5263
#define OpHitObjectExecuteShaderNV 5264
#define OpHitObjectGetCurrentTimeNV 5265
#define OpHitObjectGetAttributesNV 5266
#define OpHitObjectGetHitKindNV 5267
#define OpHitObjectGetPrimitiveIndexNV 5268
#define OpHitObjectGetGeometryIndexNV 5269
#define OpHitObjectGetInstanceIdNV 5270
#define OpHitObjectGetInstanceCustomIndexNV 5271
#define OpHitObjectGetWorldRayDirectionNV 5272
#define OpHitObjectGetWorldRayOriginNV 5273
#define OpHitObjectGetRayTMaxNV 5274
#define OpHitObjectGetRayTMinNV 5275
#define OpHitObjectIsEmptyNV 5276
#define OpHitObjectIsHitNV 5277
#define OpHitObjectIsMissNV 5278
#define OpReorderThreadWithHitObjectNV 5279
#define OpReorderThreadWithHintNV 5280
#define OpTypeHitObjectNV 5281

	// SPIR-V specific declarations for Vulkan
	// Enable SPIR-V capabilities for invocation reorder
	[[vk::ext_capability(ShaderInvocationReorderNV)]] 
	[[vk::ext_extension("SPV_NV_shader_invocation_reorder")]]

	// Define the HitObject type for SPIR-V
	[[vk::ext_type_def(HitObjectAttributeNV, OpTypeHitObjectNV)]]
	void createHitObjectNV();
	#define HitObjectNV vk::ext_type<HitObjectAttributeNV>

	// Define RayPayload storage class
	#define RayPayloadKHR 5338
#else
	#pragma shader_model 6_9
	#pragma enable_dxr_extensions
#endif
