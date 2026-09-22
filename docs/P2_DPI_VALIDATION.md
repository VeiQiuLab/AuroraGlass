# P2 — DPI / Multi-Monitor Validation (Slice 3)

Status: host-layer documentation. **Core is DPI-agnostic.**

## Pixel contract (authoritative)

- `AuroraGlass Core` `width` / `height` are **physical pixels**.
- `SurfaceDesc::width` / `SurfaceDesc::height`, `GlassSurface::Width()/Height()`,
  and `GlassSurface::Resize(w, h)` all use **physical pixels**.
- DPI, DPI scale, logical coordinates, and window positioning are the
  **Windows host / adapter** responsibility. They are never passed into Core
  and never stored in `GlassMaterial` / `GlassSurface` / `SurfaceDesc`.
- Core does not know what a "DPI" is. The host converts the actual client
  rectangle to physical pixels and calls `GlassSurface::Resize` with those.

## Host DPI awareness

`samples/p2_dpi_smoke` enables `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`
before creating the HWND. If `SetProcessDpiAwarenessContext` is unavailable
or returns failure, the sample logs the actual `GetLastError()` and states
that awareness is NOT confirmed. It never assumes success.

## WM_DPICHANGED handling

On `WM_DPICHANGED` the sample:

1. reads the new DPI from `LOWORD(wParam)`;
2. applies the `lParam`-suggested RECT via `SetWindowPos`;
3. lets the resulting `WM_SIZE` set the pending **client pixel** size;
4. applies that size exactly once in the render loop (no recursive resize).

The DPI value is used only for logging and window positioning, never for
driving Core resources.

## Owner Manual Validation (required — cannot be automated here)

Automated tests do **not** prove multi-monitor switching. The following is a
manual procedure and must be performed by the Owner on real hardware:

1. Open `build\Debug\P2_Dpi_Smoke.exe` on monitor A.
2. Record the logged DPI and client pixel size.
3. Drag the window to monitor B (a monitor with a different scale factor).
4. Confirm a `WM_DPICHANGED` line is logged.
5. Confirm the suggested window rect was applied.
6. Confirm `GlassSurface` resized to the new **client pixel** size.
7. Move back and forth between monitors at least 5 times.

Check manually:

- no crash;
- no permanent black frame;
- glass size matches the window (no misalignment);
- no obvious stretching;
- mouse highlight position remains correct;
- visuals recover correctly after each resize.

If only one monitor is available, per-monitor switching cannot be fully
exercised; record that limitation rather than claiming it passed.

## Automation honesty

Automated coverage for this slice is limited to the resize contract
(`P2_Resize_Tests`) and the host sample starting/setting awareness/logging
DPI. The cross-monitor behavior above remains an **Owner Manual Validation**
item for the P2 gate.
