# Tracy Profiler Integration for QHWI

This guide explains how to use Tracy profiler with qhwi for performance profiling.

## Building with Tracy Support

Enable Tracy when building qhwi:

```bash
cmake -DQHWI_ENABLE_TRACY=ON ...
```

## Using Tracy Profiling

### Include the Header

```cpp
#include <qhwi/qhwi_profiling.h>
```

### Basic CPU Profiling

```cpp
// Profile an entire function
void MyFunction()
{
    QHWI_PROFILE_FUNCTION();
    // Your code here...
}

// Profile a specific scope
void ComplexFunction()
{
    QHWI_PROFILE_SCOPE("Processing");
    // Processing code...
}
```

### Color-Coded Profiling
By default all (CPU) tracy zones for qhwi are white.

In the case more granularity is desired, the option is provided to use distinct colors for different categories of qhwi operation.
This is controlled by the QHWI_USE_UNIFIED_PROFILING_COLOR cmake flag.

```cpp
// Command operations - GREEN
QHWI_PROFILE_SCOPE_COLOR("CommandList::begin", QHWI_COLOR_COMMAND_LIST);

// Resource creation - BLUE  
QHWI_PROFILE_SCOPE_COLOR("Device::createBuffer", QHWI_COLOR_RESOURCE_CREATE);

// Graphics operations - RED
QHWI_PROFILE_SCOPE_COLOR("CommandList::draw", QHWI_COLOR_GRAPHICS);
```

### Available Colors

- `QHWI_COLOR_COMMAND_LIST` (Green - 0x00FF00) - Command recording operations
- `QHWI_COLOR_RESOURCE_CREATE` (Blue - 0x0000FF) - Resource creation (buffers, textures, etc.)
- `QHWI_COLOR_PIPELINE` (Magenta - 0xFF00FF) - Pipeline operations (creation, binding)
- `QHWI_COLOR_QUEUE_SUBMIT` (Orange - 0xFF8800) - Queue submissions and synchronization
- `QHWI_COLOR_TRANSFER` (Cyan - 0x00FFFF) - Transfer operations (copies, uploads)
- `QHWI_COLOR_COMPUTE` (Yellow - 0xFFFF00) - Compute dispatches and operations
- `QHWI_COLOR_GRAPHICS` (Red - 0xFF0000) - Graphics rendering (draw calls, render passes)
- `QHWI_COLOR_RAYTRACING` (Purple - 0x8800FF) - Ray tracing operations
- `QHWI_COLOR_CONTEXT` (Gray - 0x888888) - Context/Device initialization and management

When `QHWI_USE_UNIFIED_COLOR` is **enabled**, all colors default to white (0xFFFFFF) for a cleaner, unified appearance.


# GPU Profiling
Tracy GPU profiling is supported for both D3D12 and Vulkan backends.

### Enabling GPU Profiling

```
# For D3D12 GPU profling
cmake -DQHWI_ENABLE_TRACY=ON -DTRACY_GPU_BACKEND=dx12
# (QHWI_TRACY_GPU_BACKEND_VULKAN)

# For D3D12 VULKAN profling
cmake -DQHWI_ENABLE_TRACY=ON -DTRACY_GPU_BACKEND=vk
# (QHWI_TRACY_GPU_BACKEND_D3D12)
```

**Important:** The 'TRACY_GPU_BACKEND' setting must match the graphics backend the application uses at runtime.

When GPU profiling is enabled for a backend, qhwi will automatically create the necessary Tracy contexts for that backend. These are declared as member vars of the respective CommandQueue classes. (m_tracyCtx).

## GPU Profiling Usage

```cpp
void RenderFrame(qhwi::CommandQueue* queue, qhwi::CommandList* cmdList)
{
        cmdList->begin();
    
    // Profile GPU work with scoped zones
    {
        QHWI_GPU_ZONE_SCOPED(queue, cmdList, "Shadow Pass");
        // Shadow rendering commands...
    }
    
    {
        QHWI_GPU_ZONE_SCOPED(queue, cmdList, "Main Pass");
        // Main rendering commands...
    }
    
    // Dynamic zone names (runtime strings)
    std::string zoneName = "Post Process " + std::to_string(effectIndex);
    QHWI_GPU_ZONE_SCOPED_DYNAMIC(queue, cmdList, zoneName.c_str());
    // Post processing commands...
    
    cmdList->end();
    
    // Submit and collect GPU timing data
    queue->submit(1, &cmdList);
    
    // D3D12 only: Mark frame boundaries for GPU profiling
    QHWI_GPU_NEWFRAME(queue);
    
    // Collect GPU profiling data (required for both backends)
    QHWI_GPU_COLLECT(queue, cmdList);
}
```

## Backend-Specific Notes

#### Vulkan  
- `QHWI_GPU_NEWFRAME(queue)` is a no-op (not needed).
- Uses dedicated command buffers for Tracy setup to avoid interfering with main rendering.
- GPU events must be periodically collected using `QHWI_GPU_COLLECT(queue, cmdList)`. **Note:** The command buffer associated with the command list must be in the recording state and be outisde a render pass instance.
- Requires Vulkan timestamp queries and calibrated timestamps extension for best results. If VK_EXT_calibrated_timestamps is not supported/enabled, Tracy falls back to standard TracyVkContext.

### D3D12
- Requires `QHWI_GPU_NEWFRAME(queue)` to be called once per frame.
- GPU zones are automatically collected when `QHWI_GPU_COLLECT(queue, cmdList)` is called. **Note:** cmdList is not needed for D3D12 event collection, its signature remains the same as the Vulkan implementation for simplicity.
- Uses D3D12 query heaps for timing data.


### GPU Profiling Best Practices

1. **Place zones around logical GPU work units**:
   ```cpp
   QHWI_GPU_ZONE_SCOPED(queue, cmdList, "Geometry Pass");
   // All geometry rendering...
   
   QHWI_GPU_ZONE_SCOPED(queue, cmdList, "Lighting Pass");  
   // All lighting work...
   ```

2. **Avoid too many small zones** - GPU profiling has overhead
3. **Use descriptive zone names** for easier analysis
4. **Call `QHWI_GPU_COLLECT` regularly** to avoid memory buildup

## Capturing Profiles

1. Launch Tracy Profiler
2. Run your application with Tracy enabled
3. Connect to your application in Tracy
4. Both application and qhwi zones will appear in the same timeline

## Disabling Tracy

```bash
cmake -DQHWI_ENABLE_TRACY=OFF ...
```
When disabled, all `QHWI_PROFILE_*` macros become no-ops with zero overhead.

### Troubleshooting

- **Crashes on startup**: Verify `TRACY_GPU_BACKEND` matches your runtime graphics API
- **No GPU zones visible**: Ensure `QHWI_GPU_COLLECT` is called after command submission
- **Missing timing data**: Check that your GPU driver supports the required timing extensions