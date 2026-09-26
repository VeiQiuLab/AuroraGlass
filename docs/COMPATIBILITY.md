# AuroraGlass Compatibility Matrix

## Status vocabulary

- VALIDATED — exercised successfully in the current validation environment.
- SUPPORTED BY DESIGN BUT NOT VALIDATED — architecture or project configuration is intended to support it, but no current validation proof exists.
- NOT TESTED — no current execution proof.
- NOT SUPPORTED — intentionally outside the supported surface.

## Current validated environment

- Architecture: x64
- Configuration: Debug (VALIDATED) and Release (VALIDATED)
- Native graphics: Direct3D 11
- Generator: Visual Studio 17 2022 x64
- Visual Studio Build Tools: 17.14.37614.0
- MSVC: 19.44.35228.0
- Windows SDK: 10.0.26100.0
- CMake: 4.4.3
- .NET SDK: 10.0.401

The current machine is the authoritative validation environment for this Slice. Broader Windows-version claims are not implied by a single-machine validation run.

## Native / Win32

| Item | Status | Notes |
| --- | --- | --- |
| x64 | VALIDATED | Current build and tests run as x64. |
| Debug | VALIDATED | Full Debug validation baseline. |
| Release | VALIDATED | Release x64 SDK enabled and validated (build, install, external consumers, full CTest). |
| Direct3D 11 | VALIDATED | Core and Win32 sample paths exercise D3D11. |
| Hardware D3D11 device | VALIDATED | Normal execution path. |
| WARP fallback | SUPPORTED BY DESIGN BUT NOT VALIDATED | Present in the Win32 sample host fallback path. |
| Cross-monitor DPI transition | NOT TESTED | No real multi-monitor transition proof is claimed. |

## WPF

| Item | Status | Notes |
| --- | --- | --- |
| Managed target | VALIDATED | net10.0-windows. |
| x64 host | VALIDATED | Current P6 validation environment. |
| Interop boundary | VALIDATED | P6 interop tests pass. |
| Host lifecycle | VALIDATED | P6 lifecycle tests pass. |
| Metrics / input | VALIDATED | P6 metrics/input tests pass. |
| Motion managed boundary | NOT EXPOSED | Motion is not currently exposed as a managed WPF API surface. |
| Release | VALIDATED | Release WPF external consumer passes. |

## WinUI

| Item | Status | Notes |
| --- | --- | --- |
| Managed target | VALIDATED | net10.0-windows10.0.19041.0. |
| Minimum target platform | SUPPORTED BY DESIGN BUT NOT VALIDATED | Project declares 10.0.17763.0; this exact minimum OS was not validated here. |
| Runtime | VALIDATED | win-x64 in the current validation path. |
| Windows App SDK | VALIDATED | Project uses Microsoft.WindowsAppSDK 2.5.1. |
| Interop boundary | VALIDATED | P7 interop tests pass. |
| Metrics / input | VALIDATED | P7 metrics/input tests pass. |
| Material rendering | VALIDATED | P7 material render tests pass. |
| Formal sample smoke | VALIDATED | P7 formal sample smoke passes. |
| Motion managed boundary | NOT EXPOSED | Motion is not currently exposed as a managed WinUI API surface. |
| Release | VALIDATED | Release WinUI external consumer passes. |

## Release SDK validation (0.8.1)

- Release x64 SDK: VALIDATED (build, install, Debug-CRT-free interop binaries)
- Release Win32 external consumer: PASS
- Release WPF external consumer: PASS
- Release WinUI external consumer: PASS
- Release effective full CTest: 39/39 PASS
- Release interop binaries require the Microsoft Visual C++ x64 Redistributable.

## WPF D3DImage composition validation (0.9.0)

The airspace-safe WPF composition bridge (`WpfGlassImageSource`, D3D11 ->
shared BGRA -> D3D9Ex -> WPF D3DImage) is VALIDATED:

- Native composition tests: PASS
- WPF composition proof consumer: PASS
- Real hardware mouse click on WPF controls above the glass: PASS
- No HwndHost / no native child HWND in the composition path
- Real-product integration (AuroraPomodoro): PASS
- Resize: PASS
- Minimize/restore: PASS
- Tray restore: PASS

Known non-blocking behavior: `CompositionTarget.Rendering` may continue while a
consumer window is minimized unless the consumer calls `Stop()`. Consumers should
call `Stop()` when hidden/minimized and `Start()` when restored/visible. This is
a scheduling note, not a correctness bug.

## External product validation

- External product: AuroraPomodoro
- Framework: WPF
- Configuration: Debug x64
- Dependency: AuroraGlass v0.8.0 release SDK archive
- Result: Build PASS, runtime integration PASS, no AuroraGlass source-tree dependency

This is a single external product's result and does not represent all consumer
environments.

## Validation limits

This matrix records what has actually been demonstrated by the repository and the current validation environment.

It does not convert architectural intent into runtime proof. In particular:

- Debug and Release are each validated independently; validating one does not automatically validate the other.
- The declared Windows minimum version does not mean that minimum version was executed.
- Real cross-monitor DPI transitions remain untested.
- WPF and WinUI managed Motion APIs are not exposed.
