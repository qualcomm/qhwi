# qcom_tile_shading

**Android Only - Vulkan Backend**

This example demonstrates how to use VK_QCOM_tile_properties extension for tile-based rendering optimizations.

## Requirements

- **Backend**: Vulkan only
- **Platform**: Android only
- **GPU**: Qualcomm Adreno GPU with tile shading support

## Description

This example showcases tile-based rendering techniques specific to Qualcomm Adreno GPUs, utilizing the VK_QCOM_tile_properties extension to optimize rendering performance through tile memory and tile-based operations.

**Note**: This example currently uses GLSL shaders exclusively. HLSL support will be enabled once DXC properly supports the VK_QCOM_tile_shading extension.

## Command Line Parameters

See [README.md](../README.md) for shared arguments available to all examples.

| Parameter | Description | Values | Default |
|-----------|-------------|--------|---------|
| `--use-ext` | Enable tile shading extension code path | `true`, `false` | `true` |

## Running the Example

### Android
```bash
adb shell am start com.qualcomm.qhwi_example_qcom_tile_shading/android.app.NativeActivity
```

To kill the app:
```bash
adb shell am force-stop com.qualcomm.qhwi_example_qcom_tile_shading
```

Logcat:
```bash
adb logcat -T1 -s Graphics
```
