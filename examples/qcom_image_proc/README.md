# qcom_image_proc

**Vulkan Backend**

This example demonstrates how to use VK_QCOM_image_processing extension for hardware-accelerated image processing operations.

## Requirements

- **Backend**: Vulkan only
- **Platform**: Windows, Linux, Android
- **GPU**: Qualcomm Adreno GPU with image processing support

## Building

Build the example using CMake from the project root:

```bash
cmake --build build --target qhwi_example_qcom_image_proc
```

## Running the Example

### Basic Usage

```bash
./qhwi_example_qcom_image_proc [options]
```

### Command Line Parameters

See [README.md](../README.md) for shared arguments available to all examples.

| Parameter | Description | Values | Default |
|-----------|-------------|--------|---------|
| `--use-ext` | Use VK_QCOM_image_processing extension when available | `true`, `false` | `true` |
| `--kernel-type` | Filter kernel type | 0=Triangle, 1=Gaussian, 2=Mitchell-Netravali | 1 (Gaussian) |
| `--kernel-dim` | Dimensions of kernel (weight) texture | 2-16 | 9 |
| `--downscale-factor` | Width/height divisor at each downsampling step | 2-16 | 4 |
| `--thumbnail-width` | Final width for downscaled image | Integer | 255 |
| `--thumbnail-height` | Final height for downscaled image | Integer | 191 |
| `--display-mode` | Display mode | 0=Spatial sequence, 1=Temporal sequence, 2=Permutations | 1 |
| `--frame-hold` | Frames to hold each image (temporal mode) | Integer | 400 |
| `--image-steps` | Number of downsampling steps to show | ≥1 | 400 |
| `--image-file` | Image file to load (jpg/png) | Filename | "" (procedural) |
| `--shader-type` | Shader language | 0=HLSL, 1=GLSL | 1 (GLSL) |

### Example Commands

**Basic downsampling with Gaussian kernel:**
```bash
./qhwi_example_qcom_image_proc --kernel-type=1 --kernel-dim=9 --downscale-factor=4
```

**Load custom image and downsample:**
```bash
./qhwi_example_qcom_image_proc --image-file=myimage.png --thumbnail-width=128 --thumbnail-height=128
```

**Compare different kernel types:**
```bash
# Triangle kernel
./qhwi_example_qcom_image_proc --kernel-type=0 --display-mode=1

# Gaussian kernel
./qhwi_example_qcom_image_proc --kernel-type=1 --display-mode=1

# Mitchell-Netravali kernel
./qhwi_example_qcom_image_proc --kernel-type=2 --display-mode=1
```

**Show spatial sequence of downsampling stages:**
```bash
./qhwi_example_qcom_image_proc --display-mode=0
```

**Test multiple kernel/downscale combinations:**
```bash
./qhwi_example_qcom_image_proc --display-mode=2
```

## Display Modes

### Spatial Sequence (mode 0)
Displays all downsampling stages side-by-side from largest to smallest, allowing visual comparison of each stage.

### Temporal Sequence (mode 1)
Cycles through downsampling stages over time, holding each stage for the specified number of frames. Useful for detailed inspection of each level.

### Permutations (mode 2)
Displays a grid of thumbnails showing different combinations of kernel dimensions and downscale factors, useful for comparing multiple configurations.

## Kernel Types

### Triangle (0)
Simple linear interpolation kernel. Fast but may produce softer results.

### Gaussian (1)
Gaussian blur kernel with sigma based on downscale factor. Provides smooth, natural-looking downsampling.

### Mitchell-Netravali (2)
Advanced cubic filter that balances sharpness and smoothness. Often produces the highest quality results but may be slower.

## Output

The example logs performance metrics including:
- Kernel type and dimensions
- GPU downsampling time (milliseconds)
- Resolution at each downsampling step

Results are logged to console and to `log.txt` file.

## Notes

- The example automatically detects VK_QCOM_image_processing extension support
- When the extension is available, hardware-accelerated weighted average filtering is used
- Falls back to standard texture sampling when the extension is not available
- Kernel coefficients are normalized and stored as 16-bit float textures
- The number of downsampling stages is automatically calculated based on input size and target thumbnail size
