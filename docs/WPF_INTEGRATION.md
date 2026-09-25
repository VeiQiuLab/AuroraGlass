# AuroraGlass WPF Integration

## 1. Requirements

AuroraGlass P6 WPF integration currently targets Windows, WPF, and net10.0-windows.

Managed adapter:

    adapters/wpf/AuroraGlass.Wpf

Native interop library:

    AuroraGlassWpfInterop.dll

AuroraGlass v0.8.0 provides a distributable Windows x64 SDK archive.
WPF consumers can use the released SDK without referencing the AuroraGlass
source tree.

NuGet packaging is not currently provided.

See docs/SDK_ARCHIVE_README.md for the archive consumption and deployment entry point.

## 2. Architecture

Managed path:

    WPF Window
    → AuroraGlass.Wpf
    → P/Invoke
    → AuroraGlassWpfInterop.dll
    → existing Win32 integration
    → frozen AuroraGlass Core

Rendering path:

    WpfRenderHost : HwndHost
    → native child HWND
    → D3D11Device
    → GlassSurface
    → frozen GlassMaterial

WPF BlurEffect or Acrylic is not used as a substitute for the Core material model.

## 3. Native Deployment

Build the native interop target:

    cmake --build build --config Debug --target AuroraGlassWpfInterop

The Debug DLL is produced under:

    build/Debug/AuroraGlassWpfInterop.dll

The application must make the native DLL discoverable through normal Windows DLL loading.

## 4. Host Lifecycle

WpfHostAttachment is the Window attachment boundary.

Relevant API:

- Attach(Window)
- IsAttached
- Dispose()

WPF owns the Window and HWND. AuroraGlass attaches to it.

If Attach is requested before HWND creation, the adapter follows the WPF SourceInitialized lifecycle.

## 5. Metrics / DPI

Relevant API:

- CurrentMetrics
- MetricsChanged
- DipToPhysical(Point)
- DipToPhysical(Rect)

WPF layout uses DIP. AuroraGlass native rendering and input boundaries use physical client pixels where required.

Native HWND DPI remains authoritative. A 0 x 0 metric is a valid minimized state.

## 6. Material

WpfGlassMaterial maps through the P6 C ABI to the frozen Core GlassMaterial.

It does not introduce a second WPF material semantic model.

Available material operations include blur, refraction, dispersion, thickness, fresnel, specular, tint, saturation, brightness, noise, corner radius, opacity, and highlight position.

## 7. Render Host

WpfRenderHost derives from HwndHost.

It creates a native child HWND and drives the production D3D11Device and GlassSurface path.

Relevant API includes:

- SetMaterial(WpfGlassMaterial)
- SetPhysicalRects(...)
- Stats
- IsReady

## 8. Controls / Input

WpfControlInputBridge exposes:

- AddButton
- AddToggle
- AddSlider

Wrappers include WpfButton, WpfToggle, and WpfSlider.

Input remains:

    real WPF Window HWND
    → native Win32 messages
    → P5 Win32ControlInputBridge
    → frozen P3 controls

Do not create a second managed mouse-input protocol for the same controls.

## 9. Bounds

Use DipToPhysical(Point) and DipToPhysical(Rect) when explicit physical rendering bounds are required.

WPF layout coordinates remain DIP. The native boundary receives physical pixels where required.

## 10. Resize / Minimize / Restore

Consume CurrentMetrics and MetricsChanged.

The adapter supports resize, valid minimized 0 x 0 state, and restore.

Do not submit a zero drawable size to GPU work that requires a non-zero target.

## 11. Teardown

The P6 sample explicitly disposes owned adapter objects in this order:

1. WpfControlInputBridge
2. WpfHostAttachment
3. WpfGlassMaterial
4. WPF and HwndHost destruction release the render child HWND

The native path also handles WM_NCDESTROY as safety cleanup.

### Shutdown lifecycle note

The native child HWND may receive WM_NCDESTROY before Window.Closed handlers
run. Consumers should not assume WpfHostAttachment is still attached inside
Window.Closed.

Cleanup/detach must be treated as idempotent, and an already-detached state is
valid during shutdown. This is expected behavior, not a defect. The exact event
ordering is not guaranteed.

## 12. Motion Status

The P6 WPF sample does not currently expose P4 motion consumption.

P4 motion remains frozen in the native SDK, but the P6 WPF adapter does not currently expose a managed motion boundary.

No C# motion substitute is introduced, and this is not a P6 exit blocker.

## 13. Minimal Checklist

- reference AuroraGlass.Wpf
- deploy AuroraGlassWpfInterop.dll
- attach WpfHostAttachment to the WPF Window
- consume CurrentMetrics and MetricsChanged
- use DipToPhysical where physical rendering bounds are required
- create WpfGlassMaterial
- place WpfRenderHost in the visual tree
- attach WpfControlInputBridge
- register Button, Toggle, and Slider
- preserve the native P5/P3 semantic input route
- handle minimized 0 x 0
- dispose adapter ownership cleanly

## Reference Sample

The production integration sample is:

    samples/p6_wpf_sample/

It demonstrates the same frozen Core material model used by the native SDK.
