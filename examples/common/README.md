# QHWI Example Base

This directory contains the common base code (`qhwi_example.h/cpp`) used by all QHWI examples. It provides initialization, command-line argument parsing, and basic rendering loop functionality.

## Command-Line Arguments

All examples built on this base support the following command-line arguments:

| Argument | Type | Default | Description |
|----------|------|---------|-------------|
| `--backend` | string | `d3d12`/`vk`* | Graphics backend to use |
| `--width` | uint32_t | `0` | Output surface width (0 = use default) |
| `--height` | uint32_t | `0` | Output surface height (0 = use default) |
| `--debug` | bool | `true`** | Enable debug layer |
| `--apiDump` | bool | `false` | Enable API call dumping |
| `--pil-metrics` | bool | `false` | Enable real-time GPU metrics via PIL (requires `QHWI_PIL_DIR` set at build time and SDP running) |
| `--num-frames` | uint32_t | `0` | Number of frames to run before exiting (0 = run indefinitely) |

\* Default backend depends on platform/build configuration  
\*\* Default is `true` for debug builds, `false` for release builds

## Usage Examples

```bash
# Run with Vulkan backend at 1920x1080
./example_app --backend vk --width 1920 --height 1080

# Run with D3D12 backend and API dump enabled
./example_app --backend d3d12 --apiDump true

# Run with debug layer disabled
./example_app --debug false

# Combine multiple arguments
./example_app --backend vk --width 1280 --height 720 --debug true
```
