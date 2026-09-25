# AuroraGlass Samples

## Purpose

The samples are consumer-facing proofs. A formal sample should demonstrate normal consumption without reaching into production internals or depending on test-only implementation details.

## P5 Win32 Sample

Path:

samples/p5_win32_sample

Status:

- Build: VALIDATED
- Fresh scripted smoke: VALIDATED
- Internal production header dependency: NO
- Test-only dependency: NO

P8 Slice C moved the sample-owned D3D11 host plumbing into:

samples/p5_win32_sample/sample_d3d11_host.h

The formal sample no longer includes:

core/d3d11_device.h

The helper is intentionally limited to consumer-owned host responsibilities:

- D3D11 device/context creation
- swap chain ownership
- render-target-view recreation
- HWND client-size handling
- viewport setup
- Present
- hardware device creation with WARP fallback

It does not copy AuroraGlass renderer, shader-library, material, controls, motion, or Core device-lost/debug abstractions.

Validated proof:

- P5_Win32_Sample builds successfully.
- P5_Fresh_Win32_Sample_Smoke passes.

## P6 WPF Sample

Path:

samples/p6_wpf_sample

Project:

P6.WpfSample.csproj

Dependency boundary:

- References the WPF adapter project.
- No direct Core source/header dependency was found.
- No tests directory dependency was found.

Current validation proof:

- P6_Wpf_Interop_Tests: PASS
- P6_Wpf_Host_Lifecycle_Tests: PASS
- P6_Wpf_Metrics_Input_Tests: PASS
- P6_Wpf_Formal_Sample_Smoke: PASS

The formal sample builds through the P6_WPF_Sample CMake target and runs its scripted runtime path (28 checks, 0 failures): native render host ready, AuroraGlass frames presented, Core render status OK, frozen GlassMaterial, DPI conversion, frozen P3 button/toggle/slider input, resize, minimize/restore, teardown.

## P7 WinUI Sample

Path:

samples/p7_winui_sample

Project:

P7.WinUISample.csproj

Dependency boundary:

- References the WinUI adapter project.
- No direct Core source/header dependency was found.
- No tests directory dependency was found.

Current validation proof:

- P7_WinUI_Interop_Tests: PASS
- P7_WinUI_Metrics_Input_Tests: PASS
- P7_WinUI_Material_Render_Tests: PASS
- P7_WinUI_Formal_Sample_Smoke: PASS

## Maturity rules

Formal samples must remain consumer-oriented:

- no direct src/core implementation dependency
- no tests-only dependency
- no private implementation shortcut
- no duplication of AuroraGlass production rendering semantics
- sample-owned host plumbing is allowed where normal consumers would own that plumbing

P8 Slice C treats these boundaries as part of sample maturity, not merely as documentation.
