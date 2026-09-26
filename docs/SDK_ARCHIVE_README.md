# AuroraGlass 0.9.0

A reusable **Windows Liquid Glass UI SDK**.

- **Platform:** Windows x64
- **Technology:** C++20, Direct3D 11, HLSL
- **Adapters:** Win32, WPF, WinUI 3
- **License:** Apache-2.0

This archive contains the **validated Release SDK configuration**.
It is a developer SDK artifact (headers, libraries, managed assemblies,
native interop DLLs, CMake package files, shaders), not an end-user installer.

Both **Release** and **Debug** SDK configurations are validated. This archive
ships the **Release** binaries under `lib/Release/`, `bin/Release/`, and
`managed/*/Release/`.

## Native quick start (CMake)

Point CMake at the extracted SDK directory via `CMAKE_PREFIX_PATH`:

```text
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/SDK/AuroraGlass-0.9.0"
```

Then consume AuroraGlass in your `CMakeLists.txt`:

```cmake
find_package(AuroraGlass CONFIG REQUIRED)

target_link_libraries(MyApp PRIVATE
    AuroraGlass::AuroraGlassCore
    AuroraGlass::AuroraGlassMaterial
)
```

Exported native targets:

```text
AuroraGlass::AuroraGlassMaterial
AuroraGlass::AuroraGlassCore
AuroraGlass::AuroraGlassControls
AuroraGlass::AuroraGlassMotion
AuroraGlass::AuroraGlassMotionControls
AuroraGlass::AuroraGlassWin32Adapter
```

### Win32

Win32 consumers use the installed public headers (`include/`), libraries
(`lib/Release/`), and the AuroraGlass CMake targets above. For host-window
integration, link the exported adapter target:

```cmake
target_link_libraries(MyApp PRIVATE AuroraGlass::AuroraGlassWin32Adapter)
```

## WPF

A WPF application requires **all three** of the following at deployment/runtime:

```text
1. managed/WPF/Release/AuroraGlass.Wpf.dll   (managed assembly)
2. bin/Release/AuroraGlassWpfInterop.dll     (native interop DLL)
3. shaders/                                  (runtime shader resources)
```

Reference the managed assembly from your WPF application, and deploy the native
interop DLL together with the `shaders/` runtime resources next to your
application output. None of the three can be omitted. No NuGet package is used.

The Release native interop DLL requires the **Microsoft Visual C++ x64
Redistributable** to be present on the target machine.

### WPF composition: overlay WPF controls on AuroraGlass

If ordinary WPF controls must sit **above** the AuroraGlass surface, use the
airspace-safe D3DImage path (`WpfGlassImageSource`) instead of the HwndHost
path (`WpfRenderHost`). The HwndHost path renders into a native child window
that always paints above the WPF visual tree; the D3DImage path renders
offscreen and is presented as an ordinary WPF `ImageSource`, so WPF
hit-testing works normally.

```csharp
var glass = new AuroraGlass.Wpf.WpfGlassImageSource(pixelWidth, pixelHeight);
glass.SetMaterial(material);
glass.SetPhysicalRects(new System.Windows.Rect(40, 40, 520, 400));
glass.Start();  // CompositionTarget.Rendering-driven frame loop

image.Source = glass.ImageSource;      // ordinary WPF ImageSource
image.IsHitTestVisible = false;        // let WPF controls above it get clicks
```

Call `Resize(w, h)` on layout changes, and `Stop()`/`Start()` when the
window is hidden/restored. Dispose the source on teardown. The same Release
runtime files (managed assembly + native interop DLL + `shaders/`) are
required.

## WinUI 3

This archive contains the WinUI managed assembly and its native interop runtime:

```text
managed/WinUI/Release/AuroraGlass.WinUI.dll
bin/Release/AuroraGlassWinUIInterop.dll
```

Reference the managed assembly from your WinUI 3 application and deploy the
native interop DLL next to your application output. The validated environment
uses **Windows App SDK 2.5.1**.

## Runtime resources

The `shaders/` directory contains runtime resources used by production
rendering. Keep the distributed shader resources with the SDK/runtime deployment
as documented; do not omit them when copying the SDK.

## Compatibility notes

- Validated configuration: **Release and Debug / Windows x64**.
- Release SDK compatibility: **VALIDATED**.
- Real cross-monitor DPI transition: **NOT TESTED**.
- WPF and WinUI 3 managed Motion boundary: **NOT EXPOSED**.

## Full documentation

For full integration, compatibility, samples and troubleshooting guidance, see
the AuroraGlass repository documentation under `docs/`.
