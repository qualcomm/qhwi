# qcom_multiview

**Vulkan Backend**

This example demonstrates multiview rendering capabilities.

## Requirements

- **Backend**: Vulkan only
- **Platform**: Windows, Linux, Android
- **GPU**: GPU with multiview support

## Description

This example showcases multiview rendering, which allows rendering to multiple views (such as stereo rendering for VR) in a single render pass. This technique is particularly useful for VR/AR applications where the same scene needs to be rendered from multiple viewpoints efficiently.

## Command Line Parameters

See [README.md](../README.md) for shared arguments available to all examples.

| Parameter | Description | Values | Default |
|-----------|-------------|--------|---------|
| `--use-ext` | Use QCOM multiview extensions when available | `true`, `false` | `true` |

## Running the Example

### Desktop (Windows/Linux)
```bash
qhwi_example_qcom_multiview
```

### Android
```bash
adb shell am start com.qualcomm.qhwi_example_qcom_multiview/android.app.NativeActivity
```

To kill the app:
```bash
adb shell am force-stop com.qualcomm.qhwi_example_qcom_multiview
```

Logcat:
```bash
adb logcat -T1 -s Graphics
```

## Output

The example renders a scene to multiple views (2 views) in a single render pass:
- Each view uses a different viewport configuration
- The rendered views are displayed side-by-side on the screen
- The left view shows the full viewport, the right view shows a centered half-width viewport

## Notes

- The example automatically detects QCOM multiview extension support
- When QCOM extensions are available, per-view render areas are used for optimal performance
