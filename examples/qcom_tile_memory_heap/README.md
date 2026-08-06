# qcom_tile_memory_heap

**Android Only - Vulkan Backend**

This example demonstrates how to use VK_QCOM_tile_memory_heap extension for allocating textures in tile memory.

## Requirements

- **Backend**: Vulkan only
- **Platform**: Android only
- **GPU**: Qualcomm Adreno GPU with tile memory heap support

## Description

This example showcases the VK_QCOM_tile_memory_heap extension which allows textures to be allocated in fast on-chip tile memory rather than main GPU memory. This can significantly improve performance for textures that are frequently accessed during rendering.

The example loads a sample PNG image and demonstrates the performance difference between:
- Textures allocated in regular GPU memory
- Textures allocated in tile memory (when `--use-ext=true`)

## Setup

The example requires texture data to be installed:
1. Extract the data tarball
2. Run `install.ps1` to copy data files to the device

## Command Line Parameters

See [README.md](../README.md) for shared arguments available to all examples.

| Parameter | Description | Values | Default |
|-----------|-------------|--------|---------|
| `--use-ext` | Allocate texture in tile memory | `true`, `false` | `true` |

## Running the Example

### Android

**With tile memory (default):**
```bash
adb shell am start -e cmdLine "--use-ext=true" com.qualcomm.qhwi_example_qcom_tile_memory_heap/android.app.NativeActivity
```

**Without tile memory (for comparison):**
```bash
adb shell am start -e cmdLine "--use-ext=false" com.qualcomm.qhwi_example_qcom_tile_memory_heap/android.app.NativeActivity
```

To kill the app:
```bash
adb shell am force-stop com.qualcomm.qhwi_example_qcom_tile_memory_heap
```

Logcat:
```bash
adb logcat -T1 -s Graphics
```

## Notes

- The example uses a compute shader to blit the texture when tile memory is enabled
- Tile memory allocation is only available on Qualcomm Adreno GPUs with the VK_QCOM_tile_memory_heap extension
