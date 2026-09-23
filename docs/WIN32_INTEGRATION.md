# Win32 Integration

This guide documents the AuroraGlass-specific steps required to integrate the current Win32 adapter into an existing Win32 + D3D11 application.

It assumes the application already owns its window, Direct3D device/context, swap chain, and render loop. It is not a Win32 or D3D11 tutorial, and AuroraGlass does not create an application framework or take ownership of the host window.

The runnable reference implementation is:

`samples/p5_win32_sample/main.cpp`

## 1. Requirements

Current P5 integration targets:

- Windows
- C++20
- Win32
- D3D11

The consumer is expected to already own:

- a live `HWND`;
- a D3D11 device and device context;
- the swap chain / presentation loop;
- the host window lifecycle and window-placement policy.

AuroraGlass remains an embeddable SDK. It does not provide a window manager, layout engine, event bus, scene graph, MVVM runtime, or general application framework.

## 2. Link Targets

The P5 fresh sample links these AuroraGlass targets:

```cmake
target_link_libraries(YourWin32App PRIVATE
    AuroraGlassCore
    AuroraGlassControls
    AuroraGlassMotionControls
    AuroraGlassWin32Adapter
)
```

The host must also link whatever native D3D11 / Win32 libraries its own implementation requires. The repository sample additionally links `d3d11`, `dxgi`, `d3dcompiler`, and `comctl32`.

The public include roots supplied by the current CMake targets allow the sample to include:

```cpp
#include "core/glass_material.h"
#include "core/glass_surface.h"

#include "controls/control_button.h"
#include "controls/control_geometry.h"
#include "controls/control_slider.h"
#include "controls/control_toggle.h"
#include "controls/control_visual_style.h"

#include "motion/control_motion.h"

#include "win32/win32_control_input_bridge.h"
#include "win32/win32_host_attachment.h"
```

The snippets below use the same namespace context as the runnable fresh sample:

```cpp
using namespace AuroraGlass;
using namespace AuroraGlass::Adapters::Win32;
using namespace AuroraGlass::Motion;
```

## 3. Attach the Host Window

Create a `Win32HostAttachment` and attach it to the already-created host window:

```cpp
using namespace AuroraGlass::Adapters::Win32;

Win32HostAttachment host;

if (!host.Attach(hwnd)) {
    // Handle attachment failure.
}
```

The `HWND` remains owned by the application. AuroraGlass observes the window through a subclass hook; it does not replace the host WndProc or own/destroy the window.

A successful `Attach` captures the current client metrics before returning. Read them through:

```cpp
const Win32HostMetrics metrics = host.Metrics();

const std::uint32_t clientWidth = metrics.clientWidth;
const std::uint32_t clientHeight = metrics.clientHeight;
const UINT dpi = metrics.dpi;
```

While attached, the metrics snapshot is valid. A detached attachment exposes the zero snapshot.

Re-attaching the same window is idempotent. Attempting to attach a different window while already attached fails.

## 4. Resize and DPI

Register the callbacks on the host attachment:

```cpp
Win32HostMetrics pendingMetrics{};
bool hasPendingResize = false;
UINT currentDpi = host.Metrics().dpi;

host.SetResizeCallback(
    [&](Win32HostMetrics metrics)
    {
        pendingMetrics = metrics;
        hasPendingResize = true;
    });

host.SetDpiChangedCallback(
    [&](Win32HostMetrics metrics)
    {
        currentDpi = metrics.dpi;
    });
```

The adapter owns only the observation/plumbing step. The application remains responsible for deciding when and how to resize its D3D resources and window.

### Zero-size client area

A `0 x 0` client size is a valid host state, especially while minimized.

The fresh sample treats it as a suspended rendering state:

```cpp
if (metrics.clientWidth == 0 ||
    metrics.clientHeight == 0)
{
    // Suspend the GPU/Core resize and render path.
}
```

Do not forward `0 x 0` directly into a Core/GPU resize path that rejects zero-sized surfaces.

When non-zero metrics arrive again, the fresh sample resizes the host D3D resources first, then calls:

```cpp
surface.Resize(
    metrics.clientWidth,
    metrics.clientHeight);
```

and rebuilds host-owned size-dependent resources.

### WM_DPICHANGED policy

The adapter updates its DPI snapshot and invokes the registered DPI callback.

Application of the `WM_DPICHANGED__ suggested rectangle remains host policy. `Win32HostAttachment` deliberately does not call `SetWindowPos` or act as a window manager.

## 5. Create the Glass Surface

Create the surface with the host-owned D3D11 device and the non-zero initial client size:

```cpp
SurfaceDesc desc{};
desc.width = metrics.clientWidth;
desc.height = metrics.clientHeight;

GlassSurface surface;

const Status createStatus =
    GlassSurface::Create(
        device,
        desc,
        surface);

if (!createStatus.ok()) {
    // Handle initialization failure.
}
```

The host owns the D3D device. `GlassSurface` consumes it; it does not create or own the entire application graphics lifecycle.

Size changes are propagated later through `GlassSurface::Resize` after the host has handled its own D3D resize work.

## 6. Material and Controls

The fresh sample uses the public reusable material/control APIs directly.

A minimal material/style setup is:

```cpp
GlassMaterial material{};

ControlVisualStyle style{};
style.SetAll(material);
```

Create semantic controls as ordinary values and give them physical-pixel bounds:

```cpp
GlassButton button{};
GlassToggle toggle{};
GlassSlider slider{};

button.style = style;
toggle.style = style;
slider.style = style;

button.bounds = {/* x, y, width, height */};
toggle.bounds = {/* x, y, width, height */};
slider.bounds = {/* x, y, width, height */};
```

The controls are CPU-side semantic controls. The host still owns their layout and rendering.

The slider defaults to a normalized range, or a consumer can set an explicit range:

```cpp
if (!slider.SetRange(0.0f, 1.0f).ok()) {
    // Handle invalid range.
}

slider.SetValue(0.30f);
```

Minimal callbacks are available directly on the controls:

```cpp
button.onClick = []()
{
    // Activated.
};

toggle.onChanged = [](bool checked)
{
    // Checked state changed.
};

slider.onValueChanged = [](float value)
{
    // Slider value changed.
};
```

No event bus is required.

## 7. Input Bridge

Register the controls with `Win32ControlInputBridge` and attach the bridge to the same host window:

```cpp
Win32ControlInputBridge input;

if (!input.AddButton(button) ||
    !input.AddToggle(toggle) ||
    !input.AddSlider(slider))
{
    // Handle registration failure.
}

if (!input.Attach(hwnd)) {
    // Handle bridge attachment failure.
}
```

The bridge translates the Win32 mouse input required by the frozen P3 semantic controls. Its coordinate contract is Win32 client coordinates in physical pixels to AuroraGlass `ControlPoint` physical pixels.

The host does not need to manually translate `WM_LBUTTONDOWN`, `WM_LBUTTONUP`, mouse move, capture changes, or mouse-leave messages into `PointerDown`, `PointerUp`, and related control calls.

The bridge is intentionally narrow. It is not a generic Win32 message router, keyboard framework, touch/pen abstraction, layout system, visual tree, or window manager.

The bridge stores non-owning pointers to registered controls. The controls must therefore outlive their registration with the bridge.

## 8. Motion

P4 motion remains presentation-only. Semantic control state stays authoritative.

The public motion objects used by the fresh sample are:

```cpp
ButtonMotion buttonMotion;
ToggleMotion toggleMotion;
SliderMotion sliderMotion;
LightFollowMotion lightMotion;
```

Configure the existing reduced-motion capability directly:

```cpp
buttonMotion.SetReducedMotion(reducedMotion);
toggleMotion.SetReducedMotion(reducedMotion);
sliderMotion.SetReducedMotion(reducedMotion);
lightMotion.SetReducedMotion(reducedMotion);
```

Each frame, synchronize semantic state before advancing motion:

```cpp
buttonMotion.Sync(button.State());
toggleMotion.Sync(toggle.IsChecked());
sliderMotion.Sync(slider.State());

buttonMotion.Step(dt);
toggleMotion.Step(dt);
sliderMotion.Step(dt);
lightMotion.Step(dt);
```

Consume `Presentation()` for rendering only. The fresh sample uses the button presentation scale, toggle progress, slider thumb size, and light-follow position when constructing the visual presentation.

For light-follow, the host may retarget from pointer observation:

```cpp
lightMotion.RetargetPointer(pointer, bounds);

// When the pointer leaves:
lightMotion.RetargetRest();
```

This does not replace the input bridge. Light-follow pointer observation is presentation state; the bridge still owns semantic mouse dispatch to the controls.

## 9. Frame Rendering

The fresh sample follows this order:

1. prepare/draw the host-owned background into the current render target;
2. call `GlassSurface::PrepareFrame` once for the frame;
3. select the material needed for the surface/control being drawn;
4. call `GlassSurface::SetMaterial`;
5. call `GlassSurface::RenderRect` for the panel and individual control presentation rectangles;
6. present through the host-owned D3D/swap-chain path.

The essential AuroraGlass calls are:

```cpp
const Status prepareStatus =
    surface.PrepareFrame(
        context,
        backgroundSrv,
        frameInfo,
        material.GetBlurRadius());

if (!prepareStatus.ok()) {
    // Handle frame preparation failure.
}

surface.SetMaterial(material);

const Status renderStatus =
    surface.RenderRect(
        context,
        renderTargetView,
        glassRect);

if (!renderStatus.ok()) {
    // Handle rendering failure.
}
```

`PrepareFrame` prepares the per-frame glass source once. `RenderRect` then renders the requested glass rectangles using the currently selected material.

The host remains responsible for frame timing, background rendering, render-target ownership, and presentation.

## 10. Teardown

The current fresh sample demonstrates both the `WM_NCDESTROY` safety fallback and idempotent explicit cleanup.

On its normal exit path, the sample first calls `DestroyWindow(hwnd)`. The resulting `WM_NCDESTROY` causes both Win32 adapter objects to remove their subclass hooks and clear their window/attachment state automatically. The sample then calls `ShutdownAuroraGlass()`, whose `Detach()` calls are safe no-ops if the auto-detach has already occurred, before it releases AuroraGlass and host D3D resources.

Inside `ShutdownAuroraGlass()`, resource cleanup is ordered as:

1. `Win32ControlInputBridge::Detach()`;
2. `Win32HostAttachment::Detach()`;
3. release the sample background resources;
4. `GlassSurface::Reset()`;
5. clear/flush the host D3D context;
6. release the host render target, swap chain, context, device, and debug device.

For a consumer that controls shutdown before destroying the `HWND`, explicit teardown is still the recommended normal path:

```cpp
input.Detach();
host.Detach();

surface.Reset();

// Release host-owned D3D resources.
// Destroy the HWND according to the host lifecycle.
```

The `WM_NCDESTROY` handling remains the safety fallback for cases where the window is destroyed first. Consumers should not rely on AuroraGlass to own or destroy the host window.

## 11. Minimal Integration Checklist

- [ ] Link `AuroraGlassCore`, `AuroraGlassControls`, `AuroraGlassMotionControls`, and `AuroraGlassWin32Adapter`.
- [ ] Create and own the host `HWND` and D3D11 lifecycle.
- [ ] Attach `Win32HostAttachment`.
- [ ] Read initial client size and DPI from `Metrics()`.
- [ ] Configure resize and DPI callbacks.
- [ ] Create `GlassSurface` from the host D3D11 device and non-zero client size.
- [ ] Create public `GlassMaterial` / control state.
- [ ] Register controls with `Win32ControlInputBridge` and attach it.
- [ ] Create the required P4 motion objects and synchronize them from semantic state.
- [ ] Run `PrepareFrame` once per frame and `RenderRect` for the required glass rectangles.
- [ ] Treat `0 x 0` client size as a suspended host state instead of an illegal Core/GPU resize.
- [ ] Explicitly detach adapters and release AuroraGlass resources during clean teardown.

For the complete runnable implementation, see `samples/p5_win32_sample/main.cpp`.
