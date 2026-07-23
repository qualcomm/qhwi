Generate a complete, compilable qhwi example scaffold demonstrating a Vulkan or DX12 extension.

## Arguments

`$ARGUMENTS` may contain any combination of:
- **extension** — Vulkan/DX12 extension string, e.g. `VK_QCOM_tile_shading`, `VK_QCOM_image_processing`
- **name** — snake_case directory name, e.g. `qcom_tile_shading`; defaults to lowercase extension without `VK_`/`VK_KHR_`/`VK_QCOM_`/`VK_NV_` prefix
- **platform** — `vulkan`, `dx12`, `android`, or `all` (default: `all`)
- **shaders** — `hlsl`, `glsl`, or both (default: `hlsl`)

## Step 1 — Resolve arguments

Parse `$ARGUMENTS`. If extension is missing or ambiguous, ask the user before proceeding. Do not invent extension names.

Derive these symbols and use them consistently in every generated file:

| Symbol | Derivation | Example |
|---|---|---|
| `{name}` | snake_case directory name | `qcom_tile_shading` |
| `{ClassName}` | PascalCase of `{name}` + `Example` | `QcomTileShadingExample` |
| `{TargetName}` | `qhwi_example_{name}` | `qhwi_example_qcom_tile_shading` |
| `{PackageName}` | `com.qualcomm.qhwi_example_{name}` | `com.qualcomm.qhwi_example_qcom_tile_shading` |
| `{ExtString}` | The full extension string | `VK_QCOM_tile_shading` |

## Step 2 — Read reference examples

Before generating any code, read the two most relevant files from the closest existing example:

- `VK_QCOM_tile_*` → `examples/qcom_tile_shading/src/qcom_tile_shading.h` and `.cpp`
- `VK_QCOM_image_processing` → `examples/qcom_image_proc/src/qcom_image_proc.h` and `.cpp`
- `VK_QCOM_multiview_*` → `examples/qcom_multiview/src/qcom_multiview.h` and `.cpp`
- `VK_KHR_graphics_pipeline_library` → `examples/gpl/src/graphics_pipeline_library.h` and `.cpp`
- Any other extension → `examples/qcom_tile_memory_heap/src/qcom_tile_memory_heap.h` and `.cpp`

Also read `examples/CMakeLists.txt` to see the existing `add_example_dir()` calls.

Always read `examples/common/src/qhwi_example.h` to understand the base class API.

## Step 3 — Generate all scaffold files

Create every file below. Do not skip any.

---

### `examples/{name}/src/{name}.h`

```cpp
//  {name}.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
#pragma once

#include "qhwi_example.h"
#include "test_util.h"
#include <qhwi/qhwi_vk.h>  // remove if DX12-only
using namespace test_util;

namespace qhwi
{
class {ClassName} : public Example
{
public:
    struct Config : public Example::Config
    {
        // [CONFIG_FIELDS] add extension-specific config fields — see Step 4
    };

    ResultCode init(int argc, const char* const* argv);

protected:
    void recordCommandList(CommandList* commandList) override;
    void initCmdLineOpts(cxxopts::Options& opts);
    void parse(int argc, const char* const* argv, cxxopts::Options& opts,
               cxxopts::ParseResult& p, Config& config);

    Config m_config;
    cxxopts::ParseResult m_params;

private:
    // [MEMBERS] extension-specific resources — see Step 4
    qre::RefCountPtr<qhwi::DescriptorSetLayout> m_descSetLayout;
    qre::RefCountPtr<qhwi::DescriptorSet>       m_descSet;
    qre::RefCountPtr<qhwi::PipelineLayout>      m_pipelineLayout;
    qre::RefCountPtr<qhwi::GraphicsPipeline>    m_pipeline;
    test_util::Buffer1  m_bPos, m_bUv;
    test_util::Texture1 m_texture;
};
} // namespace qhwi
```

---

### `examples/{name}/src/{name}.cpp`

```cpp
//  {name}.cpp
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear

#ifdef QHWI_PLATFORM_ANDROID
#    include <qhwi_android_util.h>
using namespace qhwi_android_util;
#    include <qhwi/qhwi_android_window.h>
#endif

#include "{name}.h"
#include <qhwi/qhwi_util.h>

ResultCode {ClassName}::init(int argc, const char* const* argv)
{
    cxxopts::Options opts("{ClassName}", "Demonstrates {ExtString}");
    initCmdLineOpts(opts);
    cxxopts::ParseResult parseResult;
    parse(argc, argv, opts, parseResult, m_config);

    // [CONFIG] apply extension-specific config overrides — see Step 4
    Example::init("{ClassName}", &m_config);

    // [SUPPORT_CHECK] verify extension availability — see Step 4

    // [GEOMETRY] create vertex/index buffers
    // [TEXTURES] create textures with appropriate usage flags
    // [DESCRIPTOR_SETS] create descriptor set layout and set
    // [PIPELINES] create graphics/compute pipelines

    beginTransfer();
    // [TRANSFER] upload vertex data and textures via staging buffers
    endTransfer();

    return ResultCode::eSuccess;
}

void {ClassName}::initCmdLineOpts(cxxopts::Options& opts)
{
    Example::initCmdLineOpts(opts);
    // [OPTS] add extension-specific command-line options
}

void {ClassName}::parse(int argc, const char* const* argv,
                         cxxopts::Options& opts, cxxopts::ParseResult& p, Config& c)
{
    Example::parse(argc, argv, opts, p, c);
    // [PARSE] read extension-specific options from p into c
}

void {ClassName}::recordCommandList(CommandList* commandList)
{
    commandList->begin();
    // [RECORD_PRE] pre-render barriers or extension state setup
    beginRenderPass(commandList);
    commandList->bindGraphicsPipeline(m_pipeline.Get());
    commandList->bindGraphicsDescriptorSet(m_descSet.Get(), m_pipelineLayout.Get());
    // [RECORD_DRAW] bind vertex buffers, issue draw calls
    endRenderPass(commandList);
    // [RECORD_POST] post-render extension work
    commandList->end();
}

// ── Android entry point ──────────────────────────────────────────────────────
#ifdef QHWI_PLATFORM_ANDROID

enum class AppState { eStart, eInit, eRun };

static void onAppCmd(android_app* app, int32_t cmd)
{
    AppState* s = (AppState*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        ext::SetANativeWindow(app->window);
        *s = AppState::eInit;
    }
}

void android_main(android_app* state)
{
    FileUtil::setAndroidAssetManager(state->activity->assetManager);
    int argc; char** argv;
    GetCommandLineArgsOp op(state, "{name}", argc, argv);
    op.run();

    AppState appState = AppState::eStart;
    state->onAppCmd = onAppCmd;
    state->userData = &appState;
    {ClassName} app;
    while (true) {
        int ident = 0, events = 0;
        android_poll_source* source = nullptr;
        while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        if (appState == AppState::eInit)      { app.init(argc, (char**)argv); appState = AppState::eRun; }
        else if (appState == AppState::eRun)  { app.runIteration(); }
    }
}

#else // ── Desktop entry point ────────────────────────────────────────────────

int main(int argc, char** argv)
{
    qre::OsSetLogConsole(true);
    {ClassName} app;
    app.init(argc, argv);
    auto window = app.window();
    while (!window->shouldClose()) {
        window->pollEvents();
        app.runIteration();
    }
    return 0;
}
#endif
```

---

### `examples/{name}/CMakeLists.txt`

Use the exact pattern from `examples/qcom_tile_memory_heap/CMakeLists.txt`:

```cmake
add_qhwi_example({TargetName})
add_subdirectory(data/shaders)

if (ANDROID)
target_compile_definitions({TargetName} PRIVATE
    QHWI_DATA_DIR="/sdcard/Android/data/{PackageName}/files/data"
)
endif()

add_dependencies({TargetName} {TargetName}_shaders)
```

If the example also needs data files on desktop (e.g. it loads textures), add a desktop definition too:
```cmake
if (ANDROID)
target_compile_definitions({TargetName} PRIVATE
    QHWI_DATA_DIR="/sdcard/Android/data/{PackageName}/files/data"
)
else()
target_compile_definitions({TargetName} PRIVATE
    QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
endif()
```
Follow the reference example's pattern to decide which form to use.

---

### `examples/{name}/data/shaders/CMakeLists.txt`

Copy the exact structure from `examples/qcom_tile_memory_heap/data/shaders/CMakeLists.txt`, substituting `{TargetName}` for `qhwi_example_qcom_tile_memory_heap` everywhere. The `macro(run_command)` must reference `TARGET {TargetName}_shaders`.

```cmake
file(GLOB_RECURSE SHADER_FILES *.hlsl *.glsl *.metal *.vert *.frag)

set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")
add_custom_target({TargetName}_shaders SOURCES ${SHADER_FILES} compileShaders.py)

cmake_path(SET VULKAN_SDK $<IF:$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>,$ENV{VULKAN_SDK},"">)

set(QHWI_SPIRV_CROSS "spirv-cross" CACHE STRING "spirv-cross executable")

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../android/app/src/main/assets/shaders)
else()
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

macro(run_command)
    add_custom_command(
        TARGET {TargetName}_shaders
        POST_BUILD
        COMMAND ${ARGV}
        COMMENT "running command: ${ARGV}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )
endmacro()

find_package(Python3 COMPONENTS Interpreter)

run_command(${Python3_EXECUTABLE} ./compileShaders.py
    --systemName ${CMAKE_SYSTEM_NAME}
    $<$<BOOL:${QHWI_ENABLE_VULKAN_BACKEND}>:--enableVulkanBackend>
    $<$<BOOL:${QHWI_ENABLE_D3D12_BACKEND}>:--enableD3d12Backend>
    $<$<BOOL:${QHWI_ENABLE_METAL_BACKEND}>:--enableMetalBackend>
    --dxcSpirvPath ${QHWI_DXC}
    --dxcDxilPath  ${QHWI_DXC}
    --outputDir    ${OUTPUT_DIR}
    --sourceDir    ${CMAKE_CURRENT_SOURCE_DIR}
    --spirvCrossPath ${QHWI_SPIRV_CROSS}
)
```

---

### `examples/{name}/data/shaders/compileShaders.py`

```python
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

import os, platform, argparse, subprocess, sys, shutil
from typing import NamedTuple
sys.path.append('../../../common/scripts')
from shaderUtil import *

def main():
    init()
    # [SHADERS] call compileShaders() for each shader base name
    # Default (graphics): compileShaders('{name}')  → compiles {name}_vs.hlsl and {name}_ps.hlsl
    # Compute-only:       compileShaders('{name}', [computeShaderDesc()])
    # Mesh shader:        compileShaders('{name}', [meshShaderDesc(), pixelShaderDesc()])
    compileShaders('{name}')
main()
```

---

### Shader stubs

**IMPORTANT — sampler register binding:**
shaderUtil.py passes `-fvk-s-shift 128 0` to DXC. This means DXC shifts sampler bindings by 128 during SPIR-V compilation. In HLSL source, always write `register(s0)`, `register(s1)`, etc. — do NOT write `register(s128)`. The shift is applied automatically.

Binding conventions:
- Textures: `register(t0)`, `register(t1)`, ...
- Samplers: `register(s0)`, `register(s1)`, ...
- CBVs: `register(b0)`, `register(b1)`, ...
- UAVs (RWTexture/RWBuffer): `register(u0)`, `register(u1)`, ...

**`{name}_vs.hlsl`:**
```hlsl
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

struct VSInput
{
    float4 pos : TEXCOORD0;
    float2 uv  : TEXCOORD1;
};

struct VSOutput
{
    float2 uv  : TEXCOORD0;
    float4 pos : SV_POSITION;
};

VSOutput VSMain(VSInput i)
{
    VSOutput o;
    o.pos = i.pos;
    o.uv  = i.uv;
    return o;
}
```

**`{name}_ps.hlsl`:**
```hlsl
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

// [EXTENSION_BINDINGS] add extension-specific textures/samplers/UAVs here
Texture2D<float4> g_Tex     : register(t0);
SamplerState      g_Sampler : register(s0);

struct PSInput  { float2 uv : TEXCOORD0; };

float4 PSMain(PSInput i) : SV_TARGET
{
    return g_Tex.Sample(g_Sampler, i.uv);
}
```

For compute-only extensions generate `{name}_cs.hlsl` instead:
```hlsl
// [EXTENSION_BINDINGS]
RWTexture2D<float4> g_Output : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    g_Output[id.xy] = float4(1, 0, 0, 1);
}
```

---

### `examples/{name}/README.md`

```markdown
# {name}

Demonstrates [{ExtString}](link-to-spec-if-known).

## Build

CMake from repository root:

```
cmake -B build -DQHWI_BUILD_EXAMPLES=ON -DQHWI_EXAMPLES_FILTER={name}
cmake --build build --target {TargetName}
```

## Run (desktop)

```
./{TargetName} [--backend vk|d3d12] [--debug] [--width W] [--height H]
```

## Run (Android)

Start:
```
adb shell am start {PackageName}/android.app.NativeActivity
```

With arguments:
```
adb shell am start -e cmdLine "--<arg>=<value>" {PackageName}/android.app.NativeActivity
```

Stop:
```
adb shell am force-stop {PackageName}
```

Logcat:
```
adb logcat -T1 -s Graphics
```
```

---

### Android build files

**`examples/{name}/android/build.gradle`** — copy verbatim from `examples/qcom_tile_memory_heap/android/build.gradle` (identical across all examples).

**`examples/{name}/android/settings.gradle`:**
```groovy
pluginManagement {
    repositories { gradlePluginPortal(); google(); mavenCentral() }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories { google(); mavenCentral() }
}
include ':app'
```

**`examples/{name}/android/gradle.properties`:**
```properties
org.gradle.jvmargs=-Xmx16g -Dfile.encoding=UTF-8
android.useAndroidX=true
kotlin.code.style=official
android.nonTransitiveRClass=true
```

**`examples/{name}/android/app/build.gradle`** — use the exact pattern from `examples/qcom_tile_memory_heap/android/app/build.gradle` with substitutions:

```groovy
plugins {
    id 'com.android.application'
}

android {
    compileSdk 34
    ndkVersion '27.0.12077973'

    defaultConfig {
        applicationId = '{PackageName}'
        minSdkVersion 24
        targetSdkVersion 34
        externalNativeBuild {
            cmake {
                arguments '-DANDROID_STL=c++_shared','-DQHWI_BUILD_EXAMPLES=ON','-DQHWI_EXAMPLES_FILTER={name}'
            }
        }
        ndk {
            abiFilters "arm64-v8a"
        }
    }
    buildTypes {
        release {
            minifyEnabled false
            proguardFiles getDefaultProguardFile('proguard-android.txt'), 'proguard-rules.pro'
            signingConfig signingConfigs.debug
        }
    }
    externalNativeBuild {
        cmake {
            version '3.24.1+'
            path '../../../../CMakeLists.txt'
        }
    }
    namespace "{PackageName}"
}

dependencies {
    implementation fileTree(dir: 'libs', include: ['*.jar'])
    implementation 'androidx.appcompat:appcompat:1.0.2'
    implementation 'androidx.constraintlayout:constraintlayout:1.1.3'
}
```

**IMPORTANT:** `QHWI_EXAMPLES_FILTER` must be `{name}` (e.g. `qcom_tile_shading`), NOT `{TargetName}`.

**`examples/{name}/android/app/src/main/AndroidManifest.xml`:**
```xml
<?xml version="1.0" encoding="utf-8"?>
<!-- BEGIN_INCLUDE(manifest) -->
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    android:versionCode="1"
    android:versionName="1.0">
  <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE"/>
  <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE"/>
  <uses-permission android:name="android.permission.READ_MEDIA_IMAGES"/>

  <application
      android:allowBackup="false"
      android:fullBackupContent="false"
      android:icon="@mipmap/ic_launcher"
      android:label="@string/app_name"
      android:hasCode="false"
      android:theme="@style/Application.Fullscreen">
    <activity android:name="android.app.NativeActivity"
              android:label="@string/app_name"
              android:configChanges="orientation|keyboardHidden"
              android:exported="true"
              android:screenOrientation="landscape">
      <meta-data android:name="android.app.lib_name"
                 android:value="{TargetName}" />
      <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
    </activity>
  </application>
</manifest>
<!-- END_INCLUDE(manifest) -->
```

**`examples/{name}/android/app/src/main/res/values/strings.xml`:**
```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="app_name">{name}</string>
</resources>
```

**`examples/{name}/android/app/src/main/res/values/themes.xml`** — read `examples/qcom_tile_shading/android/app/src/main/res/values/themes.xml` and copy its content verbatim.

---

## Step 4 — Extension-aware code substitutions

Expand the `[...]` regions in the generated code based on `{ExtString}`.

### VK_QCOM_tile_shading

**[CONFIG_FIELDS]:** (none needed — tile shading is enabled via qgl_config.txt)

**[CONFIG] in init():**
```cpp
m_config.autoBarrier = false;
std::array swapchainFormatCandidates = { qhwi::Format::eRgba8Unorm };
m_config.swapchainFormatCandidates = swapchainFormatCandidates;
m_config.swapchainImageUsage |= TextureUsage_Storage;
```

**[SUPPORT_CHECK]:** (none — tile shading requires qgl_config.txt on device)

**[MEMBERS]** — add to class:
```cpp
qre::RefCountPtr<qhwi::ComputePipeline>      m_csPipeline;
qre::RefCountPtr<qhwi::DescriptorSetLayout>  m_csDescSetLayout;
qre::RefCountPtr<qhwi::DescriptorSet>        m_csDescSet;
qre::RefCountPtr<qhwi::PipelineLayout>       m_csPipelineLayout;
test_util::Buffer1 m_transientBuffer;
```

**[RECORD_PRE] + [RECORD_POST]** — wrap per-tile compute in `VkCommandList`:
```cpp
qhwi::VkCommandList* vkCmdList = qhwi::vk(commandList);
// ... graphics render pass draws geometry ...
vkCmdList->beginPerTileExecutionQCOM();
commandList->bindComputePipeline(m_csPipeline.Get());
commandList->bindComputeDescriptorSet(m_csDescSet.Get(), m_csPipelineLayout.Get());
commandList->addTextureBarrier({ .texture = m_swapchain->currentTexture(),
    .src = TextureState_ColorAttachment,
    .dst = TextureState_ComputeShader_TileTransientReadWriteQcom }, DependencyFlag_ByRegion);
vkCmdList->dispatchTileQCOM();
vkCmdList->endPerTileExecutionQCOM();
```

**Extra file — `examples/{name}/android/qgl_config.txt`:**
```
0x00000000=0x08675309

VK_QCOM_tile_shading=TRUE

MaxBinWidth=128
MaxBinHeight=128
```

**Shader note:** The compute shader for per-tile work uses `[[vk::tile_attachment_qcom]]` on input images and `TileOffsetQCOM`/`TileDimensionQCOM` builtins. Read `examples/qcom_tile_shading/` for the exact compute shader patterns.

**CMakeLists.txt (top-level):** Android-only — wrap in `if(ANDROID)`.

---

### VK_QCOM_tile_memory_heap

**[CONFIG_FIELDS]:**
```cpp
bool enableVkQcomTileMemoryHeap = true;
```

**[CONFIG] in init():**
```cpp
m_config.enableVkQcomTileMemoryHeap = true;
std::array swapchainFormatCandidates = { qhwi::Format::eRgba8Unorm };
m_config.swapchainFormatCandidates = swapchainFormatCandidates;
m_config.swapchainImageUsage |= TextureUsage_Storage;
```

**[TEXTURES]** — add `TextureUsage_TileMemoryQCOM` to tile-resident textures:
```cpp
m_texture.init(m_device.Get(),
    TextureDesc().setWidth(W).setHeight(H)
        .setUsage(TextureUsage_Sampled | TextureUsage_Storage | TextureUsage_TileMemoryQCOM)
        .setAutomaticBarriers(true));
```

**Extra file — `examples/{name}/android/qgl_config.txt`:**
```
0x00000000=0x08675309

VK_QCOM_tile_memory_heap=TRUE
```

**CMakeLists.txt (top-level):** Android-only — wrap in `if(ANDROID)`.

---

### VK_QCOM_image_processing

**[SUPPORT_CHECK]:**
```cpp
if (!m_device->supportsImageProcessingQCOM()) {
    QHWI_LOGI("{ExtString} not supported on this device; using fallback path");
}
```

**[DESCRIPTOR_SETS]** — branch on support:
```cpp
if (m_device->supportsImageProcessingQCOM()) {
    m_descSetLayout = m_device->createDescriptorSetLayout({ DescriptorSetLayoutDesc()
        .addTexture(0, ShaderStage_Pixel)
        .addSampler(0, ShaderStage_Pixel)
        .addSampleWeightImageQCOM(1, ShaderStage_Pixel)
        .addSampler(1, ShaderStage_Pixel) });
    m_kernelSamplerDesc.flags = SamplerCreateFlag_ImageProcessingQCOM;
    m_kernelSamplerDesc.reductionMode = SamplerReductionMode::eWeightedAverage;
} else {
    // plain texture path
}
```

**[TEXTURES]** — kernel texture needs `TextureUsage_SampleWeightQCOM` when supported:
```cpp
TextureUsageMask kernelUsage = TextureUsage_Sampled;
if (m_device->supportsImageProcessingQCOM())
    kernelUsage |= TextureUsage_SampleWeightQCOM;
m_textureKernel.init(m_device.Get(),
    TextureDesc().setWidth(kernelW).setHeight(kernelH)
        .setFormat(Format::eR16Float)
        .setType(TextureType::e2DArray)
        .setUsage(kernelUsage)
        .setAutomaticBarriers(false));
```

**CMakeLists.txt (top-level):** Cross-platform — add unconditionally.

---

### VK_QCOM_multiview_per_view_render_areas / VK_QCOM_multiview_per_view_viewports

**[MEMBERS]** — add:
```cpp
static constexpr uint32_t NumViews = 2;
test_util::Texture1 m_multiViewTexture;
```

**[TEXTURES]:**
```cpp
m_multiViewTexture.init(m_device.Get(),
    TextureDesc().setWidth(W).setHeight(H)
        .setType(TextureType::e2DArray).setArrayLayers(NumViews)
        .setUsage(TextureUsage_ColorAttachment | TextureUsage_Sampled)
        .setAutomaticBarriers(true));
```

**[RECORD_DRAW]** — call the multiview render pass helper. Read `examples/qcom_multiview/src/qcom_multiview.cpp` for the exact `beginMultiViewRenderPass()` call signature.

**CMakeLists.txt (top-level):** Cross-platform — add unconditionally.

---

### VK_QCOM_tile_shading

**[SUPPORT_CHECK]:** No explicit device check — tile shading is assumed on supported Qualcomm Android targets.

**[MEMBERS]** — add:
```cpp
test_util::Texture1                      m_offscreenTexture;  // array texture for color + debug
qre::RefCountPtr<qhwi::Buffer>           m_transientBuffer;   // tile transient storage
qre::RefCountPtr<qhwi::Buffer>           m_computeBuffer;     // compute shader output
static constexpr qhwi::Extent2D          s_TileApronSize = {2, 2};
```

**[CONFIG] in init():** Set `TextureUsage_ColorAttachment | TextureUsage_Storage` on the offscreen texture, and bind the transient buffer to the compute descriptor set.

**[RENDER_PASS]:** Pass `TileShadingInfoQCOM` as the 6th argument to `beginRenderPass()`:
```cpp
cmdList->beginRenderPass(...,
    qhwi::TileShadingInfoQCOM().setEnableTileShading(true).setTileApronSize(s_TileApronSize));
```

**[RECORD]:** Wrap per-tile compute work between the tile execution scope calls:
```cpp
vkCommandList->beginPerTileExecutionQCOM();
cmdList->setState(m_offscreenTexture, TextureState_ComputeShader_TileTransientReadWriteQcom);
vkCommandList->dispatchTileQCOM();
vkCommandList->endPerTileExecutionQCOM();
```

Read `examples/qcom_tile_shading/src/qcom_tile_shading.cpp` for the exact descriptor binding and multi-pass tile compute setup.

**CMakeLists.txt (top-level):** Android-only — add inside the `if(ANDROID)` block.

---

### VK_KHR_graphics_pipeline_library

**[MEMBERS]** — add library handles (read `gpl/` for exact types):
```cpp
qre::RefCountPtr<qhwi::GraphicsPipeline> m_vertexInputLib;
qre::RefCountPtr<qhwi::GraphicsPipeline> m_preRasterizationLib;
qre::RefCountPtr<qhwi::GraphicsPipeline> m_fragmentLib;
qre::RefCountPtr<qhwi::GraphicsPipeline> m_fragmentOutputLib;
```

**[PIPELINES]** — create four library stages then link. Mirror the exact QHWI API calls from `examples/gpl/src/graphics_pipeline_library.cpp`:
```cpp
m_vertexInputLib      = m_device->createGraphicsPipelineLibrary(vertexInputDesc);
m_preRasterizationLib = m_device->createGraphicsPipelineLibrary(preRasterDesc);
m_fragmentLib         = m_device->createGraphicsPipelineLibrary(fragDesc);
m_fragmentOutputLib   = m_device->createGraphicsPipelineLibrary(fragOutputDesc);
m_pipeline = m_device->linkGraphicsPipelineLibraries(
    { m_vertexInputLib, m_preRasterizationLib, m_fragmentLib, m_fragmentOutputLib });
```

**CMakeLists.txt (top-level):** Cross-platform — add unconditionally.

---

### DX12 (mesh shaders / enhanced barriers / DirectStorage)

**[CONFIG] in init():**
```cpp
m_config.backend = GraphicsBackend::eD3D12;  // set as default, overridable via --backend
```

For mesh shaders, use `meshShaderDesc()` in `compileShaders.py`:
```python
compileShaders('{name}', [meshShaderDesc(), pixelShaderDesc()])
```

For DirectStorage, include `<dstorage.h>` and use the QHWI DirectStorage API. Verify availability with the relevant `m_device->supports*()` call.

**CMakeLists.txt (top-level):** Windows-only — wrap in `if(WIN32)` or `if(QHWI_ENABLE_D3D12_BACKEND)`.

---

## Step 5 — Update `examples/CMakeLists.txt`

Add `add_example_dir({name})` in the appropriate location:

- **Android-only** (`VK_QCOM_tile_shading`, `VK_QCOM_tile_memory_heap`, and any extension requiring Adreno tile memory):
  ```cmake
  if(ANDROID)
      add_example_dir({name})
  endif()
  ```
  Place it inside the existing `if(ANDROID)` block.

- **Cross-platform** (all other extensions): add unconditionally near similar examples:
  ```cmake
  add_example_dir({name})
  ```

- **DX12-only** or **Windows-only**:
  ```cmake
  if(QHWI_ENABLE_D3D12_BACKEND)
      add_example_dir({name})
  endif()
  ```

---

## Step 6 — Consistency verification

Before writing any file, verify:

1. Every `{name}`, `{ClassName}`, `{TargetName}`, `{PackageName}` occurrence is consistent across all files
2. `android.app.lib_name` in `AndroidManifest.xml` = `{TargetName}` (e.g. `qhwi_example_qcom_tile_shading`)
3. `applicationId` and `namespace` in `app/build.gradle` = `{PackageName}`
4. `QHWI_EXAMPLES_FILTER` in `app/build.gradle` = `{name}` (NOT `{TargetName}`)
5. `add_custom_target` name in `data/shaders/CMakeLists.txt` = `{TargetName}_shaders`
6. `TARGET` in `macro(run_command)` = `{TargetName}_shaders`
7. `add_dependencies` in `examples/{name}/CMakeLists.txt` references `{TargetName}_shaders`
8. `compileShaders.py` `compileShaders(...)` calls list only the base names of shader files actually created (no path, no extension)
9. Shader `register(sN)` uses N=0,1,2… in HLSL source (shift applied by compiler)

---

## Step 7 — Common pitfalls (learned from real builds)

### test_util::Buffer1 API — correct method names

Use the actual Buffer1 API. Do NOT invent method names:

| Correct | Wrong (does not exist) |
|---|---|
| `initAsUnorderedAccessBuffer(device, name, data, size)` | `initAsStorageBuffer(...)` |
| `initAsVertexBuffer(device, name, data, size)` | |
| `initAsConstantBuffer(device, name, data, size)` | `initStagingBuffer(...)` |

For storage/UAV buffers: pass CPU data directly to `initAsUnorderedAccessBuffer(device, name, data_ptr, byte_size)`. The method internally creates a staging buffer. Then call `uploadFromStagingBuffer(cmdList)` inside `beginTransfer()`/`endTransfer()`.

For output-only buffers (compute writes, no initial CPU data): pass `nullptr` as data.

### DescriptorSetDesc — addRWByteAddressBuffer vs addStorageBuffer

`DescriptorSetDesc::addStorageBuffer(register, buffer, **strideBytes**, ...)` requires a mandatory `strideBytes` argument. For raw byte-address buffers (HLSL `RWByteAddressBuffer`), use the stride-free overload instead:

```cpp
// CORRECT — for RWByteAddressBuffer bindings:
m_descSet = m_device->createDescriptorSet(
    DescriptorSetDesc(m_descSetLayout)
        .addRWByteAddressBuffer(0, m_bInput.get())
        .addRWByteAddressBuffer(1, m_bOutput.get())
        .setName("..."));

// WRONG — addStorageBuffer needs a mandatory third arg (strideBytes):
// .addStorageBuffer(0, m_bInput.get())  ← compile error
```

`DescriptorSetLayoutDesc::addStorageBuffer(register, shaderStage)` (2 args) IS valid for the layout; the per-set description is where the stride issue arises.

### Non-virtual base class methods — no `override`

Only `recordCommandList` is `virtual` in `Example`. Do NOT add `override` to:
- `initCmdLineOpts(cxxopts::Options&)`
- `parse(int, const char* const*, cxxopts::Options&, cxxopts::ParseResult&, Config&)`

### CMakeLists.txt — single QHWI_DATA_DIR with if/else

**WRONG** (causes duplicate macro definition on Android):
```cmake
target_compile_definitions(... QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data")
if(ANDROID)
    target_compile_definitions(... QHWI_DATA_DIR="/sdcard/...")
endif()
```

**CORRECT** — use `if/else`:
```cmake
if(ANDROID)
    target_compile_definitions(... QHWI_DATA_DIR="/sdcard/Android/data/{PackageName}/files/data")
else()
    target_compile_definitions(... QHWI_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data")
endif()
```

### Android main loop — check init() return code

When `init()` returns `eUnsupported` (extension not available), the app must NOT enter the render loop or `runIteration()` will assert on uninitialized pipelines. Always check:

```cpp
if (appState == AppState::eInit)
{
    ResultCode rc = app.init(argc, (char**)argv);
    appState = (rc == ResultCode::eSuccess) ? AppState::eRun : AppState::eStart;
}
```

Apply the same guard in `main()` on desktop:
```cpp
if (app.init(argc, argv) != ResultCode::eSuccess)
    return 1;
```

### gradle-wrapper.jar generation

The android directory does not store the binary `gradle-wrapper.jar` (blocked by QCOM GitHub policy). It is generated from `gradle/wrapper/genwrapper.py` which writes the jar bytes to stdout. Generate it before the first `gradlew.bat` invocation:

```python
# From the android/ directory:
python -c "
import subprocess
result = subprocess.run(['python', 'gradle/wrapper/genwrapper.py'], capture_output=True)
open('gradle/wrapper/gradle-wrapper.jar', 'wb').write(result.stdout)
"
```

### JDK compatibility for Gradle 8.3

Gradle 8.3 requires JDK ≤ 21. If the system Java is JDK 25+, set `JAVA_HOME` to Android Studio's bundled JBR:

```powershell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
```

### Extension support check — use supportsExtension()

For extensions not yet wrapped in `VkFeaturesInfo`, use `VkDevice::supportsExtension(name)`:

```cpp
#ifdef QHWI_BACKEND_VULKAN
{
    auto* vkDev = qhwi::vk(m_device.Get());
    bool supported = vkDev->supportsExtension("VK_NV_cooperative_vector");
    QHWI_LOGI("VK_NV_cooperative_vector: %s (device: %s)",
               supported ? "supported" : "NOT supported",
               vkDev->getPhysicalDeviceProperties().deviceName.data());
    if (!supported)
        return ResultCode::eUnsupported;
}
#else
    return ResultCode::eUnsupported;
#endif
```

Extensions already wrapped in `VkFeaturesInfo` (and exposed on `Device`) should use the dedicated method, e.g. `m_device->supportsOpticalFlowNV()`.
