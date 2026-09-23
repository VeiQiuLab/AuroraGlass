# WinUI 3 Integration

## Requirements

- Windows
- WinUI 3
- Windows App SDK 2.5.1
- net10.0-windows10.0.19041.0
- WindowsPackageType None
- AuroraGlass.WinUI
- AuroraGlassWinUIInterop.dll

This document describes the current source/build consumption path only.
P7 does not define a future NuGet or packaging contract.

## Architecture

Host and input:

~~~
WinUI 3 Window
-> WindowNative.GetWindowHandle
-> AuroraGlass.WinUI
-> AuroraGlassWinUIInterop.dll
-> frozen P5 host/input
-> frozen P3 controls
-> AuroraGlass Core
~~~

Rendering:

~~~
WinUI top-level HWND
-> P7 rendering boundary
-> native child HWND renderer
-> D3D11Device
-> GlassSurface
-> frozen GlassMaterial
~~~

The renderer is not Acrylic, Mica, Win2D, or a duplicated managed material implementation.

## Window ownership and lifecycle

WinUI owns the Window and top-level HWND.

AuroraGlass attaches through WinUIHostAttachment and does not replace WinUI window ownership.

Relevant APIs include Attach, Detach, IsAttached, WindowHandle, CurrentMetrics, MetricsChanged, and LogicalToPhysical.

## Metrics and DPI

Authoritative DPI comes from the real HWND through the frozen P5 Win32HostMetrics path.

XamlRoot.RasterizationScale is only a cross-check or integration aid.

A zero-sized client area is valid while minimized.

Real cross-monitor DPI transition behavior is NOT TESTED.

## Coordinate contract

~~~
WinUI logical effective pixels
-> LogicalToPhysical
-> physical client pixels
-> frozen P3 control geometry
~~~

Do not maintain a second independent DPI or coordinate tracker.

## Material

WinUIGlassMaterial forwards through the P7 native boundary to the existing frozen material ABI and frozen GlassMaterial.

P7 does not redefine material defaults, validation, or equations.

## Render host

WinUIRenderHost uses the existing AuroraGlass renderer.

~~~
native child HWND
-> D3D11Device
-> GlassSurface
-> frozen GlassMaterial
~~~

The top-level HWND remains WinUI-owned.

## Controls and input

P7 exposes WinUIControlInputBridge, WinUIButton, WinUIToggle, WinUISlider, AddButton, AddToggle, and AddSlider.

Input remains:

~~~
real WinUI HWND
-> Win32 messages
-> frozen P5 input bridge
-> frozen P3 controls
~~~

Do not duplicate the same control input through WinUI pointer events.

## Resize, minimize and restore

Use CurrentMetrics and MetricsChanged.

The formal sample validates resize, minimized zero size, restore, and rendering after restore.

## Teardown

The formal sample confirms this cleanup order:

1. Dispose WinUIRenderHost.
2. Dispose WinUIGlassMaterial.
3. Detach WinUIControlInputBridge.
4. Dispose WinUIControlInputBridge.
5. Detach WinUIHostAttachment.
6. Dispose WinUIHostAttachment.
7. Allow the WinUI-owned Window and top-level HWND to close normally.

Disposing WinUIRenderHost releases the AuroraGlass rendering boundary, including its owned native child HWND and rendering resources, before input and host attachment teardown.

The top-level Window and HWND remain owned by WinUI.

## Motion

P4 motion remains frozen in the native SDK.

The WinUI 3 managed motion boundary is currently NOT EXPOSED.

This is not a P7 blocker.

## Integration checklist

- create the WinUI Window
- obtain the real HWND through WindowNative.GetWindowHandle
- attach WinUIHostAttachment
- use CurrentMetrics and MetricsChanged
- convert logical geometry with LogicalToPhysical
- create WinUIGlassMaterial
- create WinUIRenderHost
- register controls through WinUIControlInputBridge
- preserve the existing P5 to P3 input path
- handle resize, minimize, and restore
- dispose AuroraGlass-owned resources cleanly

See samples/p7_winui_sample for the formal production example.