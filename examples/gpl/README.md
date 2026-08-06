# Graphics Pipeline Library (GPL) Example

This example demonstrates Vulkan Graphics Pipeline Library functionality, comparing monolithic pipeline creation versus pipeline library creation with various configuration options.

## Overview

The GPL example tests pipeline creation and rendering performance by:
- Creating graphics pipelines using either monolithic or library approaches
- Rendering a rotating colored cube
- Measuring pipeline creation time and GPU rendering performance
- Supporting various shader complexity levels and optimization strategies

## Requirements

- Vulkan support with `VK_EXT_graphics_pipeline_library` extension
- The example will exit if GPL support is not available on the GPU

## Building

Build the example using CMake from the project root:

```bash
cmake --build build --target qhwi_example_gpl
```

## Running the Example

### Basic Usage

```bash
./qhwi_example_gpl [options]
```

### Command Line Parameters

See [README.md](../README.md) for shared arguments available to all examples.

| Parameter | Description | Values | Default |
|-----------|-------------|--------|---------|
| `--use-ext` | Default pipeline type: true=pipeline library, false=monolithic (overridden by `--pipelineType`) | `true`, `false` | `true` |
| `--shaderVariation` | Shader complexity type | 0=permutations, 1=deadVarying, 2=HeavyVec4, 3=largeUBO | 0 |
| `--numPipelines` | Number of pipelines to create | Integer | 1024 |
| `--pipelineType` | Pipeline creation method | 0=monolithic, 1=pipeline library | 0 |
| `--retainLinkTimeOpt` | Retain link time optimization | true/false | true |
| `--linkTimeOpt` | Enable link time optimization | true/false | true |
| `--runTimePipelines` | Create pipelines at runtime | true/false | false |
| `--numFramesToRender` | Number of frames to render | Integer | 10000 |
| `--numFramesPerPipeline` | Frames per pipeline change | Integer | 10 |
| `--maxNumShaders` | Maximum number of shaders (encourages reuse) | Integer | 10 |
| `--vsync` | Enable VSync | true/false | false |
| `--testConfig` | Use pre-configured test | 0-10 or -1 for custom | -1 |
| `--shareShaders` | Share shaders between pipeline libraries | true/false | false |
| `--usePipelineCache` | Use pipeline cache when creating pipelines | true/false | true |
| `--smallViewport` | Render to a small viewport (32x32) | true/false | false |

### Example Commands

**Compare monolithic vs pipeline library creation:**
```bash
# Monolithic pipelines
./qhwi_example_gpl --pipelineType=0 --numPipelines=100 --numFramesToRender=1000

# Pipeline library
./qhwi_example_gpl --pipelineType=1 --numPipelines=100 --numFramesToRender=1000
```

**Test with different shader complexities:**
```bash
# Permutation shaders
./qhwi_example_gpl --shaderVariation=0 --maxNumShaders=10

# Heavy Vec4 shaders
./qhwi_example_gpl --shaderVariation=2
```

**Runtime pipeline creation:**
```bash
./qhwi_example_gpl --runTimePipelines=true --numPipelines=100 --numFramesPerPipeline=10
```

**Use pre-configured test:**
```bash
./qhwi_example_gpl --testConfig=0
```

## Output

The example logs performance metrics including:
- GPU properties (fast linking, independent interpolation decoration)
- Configuration settings
- Pipeline library creation time (ms)
- Pipeline creation time (ms)
- Total rendering time (ms)
- GPU rendering time per frame (ms)

Results are logged to console and to `gpl.log` file.

## Notes

- The example automatically exits after rendering the specified number of frames
- Shader files must be present in the `QHWI_DATA_DIR` (configured via CMakeLists.txt)
- The example renders a rotating colored cube to visualize the pipeline switching
