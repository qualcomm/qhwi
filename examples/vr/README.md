# vr (Virtual Reality)

**Multi-Backend Support: Vulkan and DirectX 12**

This example demonstrates OpenXR integration with QHWI for VR rendering.

## Requirements

- **Backend**: Vulkan or DirectX 12
- **Platform**: Windows (D3D12), Linux (Vulkan), Android (Vulkan)
- **VR Runtime**: OpenXR-compatible VR runtime (e.g., SteamVR, Oculus Runtime)

### Android: `libvrapi.so`

The Android build loads Meta's VrApi library (`libvrapi.so`) at runtime, but it is **not** bundled in this repository — VrApi is distributed under Meta's own Oculus Mobile SDK license, which does not permit redistribution outside Meta's own app packaging. To build the Android target:

1. Download the Oculus Mobile SDK from the [Meta Quest developer site](https://developer.oculus.com/downloads/package/oculus-mobile-sdk/).
2. Copy `VrApi/Libs/Android/arm64-v8a/libvrapi.so` from the SDK into `examples/vr/android/app/src/main/jniLibs/arm64-v8a/libvrapi.so` in your local checkout.
3. Build as normal — this path is already excluded from git via `.gitignore`.

## Description

This example showcases VR rendering using OpenXR, the open standard for VR and AR applications. The example demonstrates:
- OpenXR session creation and management
- Swapchain handling for VR displays
- Stereo rendering for left and right eye views
- Integration with both Vulkan and DirectX 12 backends

The example supports both Vulkan and DirectX 12 graphics backends, making it one of the few examples that is not limited to a single backend.

## Command Line Parameters

| Parameter | Description | Values | Default |
|-----------|-------------|--------|---------|
| `--backend` | Graphics backend | `vk`, `d3d12` | `d3d12` (Windows), `vk` (Linux/Android) |
| `--debug` | Enable debug layer | `true`, `false` | `true` (debug), `false` (release) |

## Running the Example

### Desktop (Windows/Linux)
```bash
# Vulkan backend
qhwi_example_vr --backend=vk

# DirectX 12 backend (Windows only)
qhwi_example_vr --backend=d3d12
```

### Android
```bash
adb shell am start com.qualcomm.qhwi_example_vr/android.app.NativeActivity
```

To kill the app:
```bash
adb shell am force-stop com.qualcomm.qhwi_example_vr
```

Logcat:
```bash
adb logcat -T1 -s Graphics
```

## Notes

- Requires an OpenXR runtime to be installed and configured.
- Requires VR headset (or suitable simulator) to be connected and configured.
