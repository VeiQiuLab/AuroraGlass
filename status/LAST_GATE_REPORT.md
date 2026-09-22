# AuroraGlass — Gate Report (Last Gate)

**Last updated:** 2026-09-22
**Active Phase:** P1 — Core Library
**P1 Gate Result:** APPROVED / FROZEN (Owner Final Approval 2026-09-22)
**P0 Phase Result:** APPROVED / FROZEN
**Next Phase:** P2 — Core Stabilization (NOT started)

> This report reflects the actual workspace state as verified by the
> handover audit. It is not a transcription of prior agent claims.
> Only commands that were actually executed and observed are recorded
> as evidence.

---

## P0 — Visual Core Proof: APPROVED / FROZEN

- Owner Visual Review: PASS.
- Frozen golden baseline retained at `samples/p0_proof` — must never be
  modified, deleted, or rewritten.
- `samples/p0_proof/main.cpp` still consumes the P0-internal pipeline
  (`GlassRenderer` / `GlassMaterialParams` / `StageFlags`); it has NOT been
  migrated to the P1 public API. Its source mtime precedes all P1 files.
- Note: this workspace is NOT a git repository, so P0 frozen status is
  verified by source content and file timestamps, not by commit history.

---

## P1 — Core Library: REVIEW

### Owner Validation

- Owner Visual Parity = **PASS** (P0_Proof vs P1_Proof compared by eye;
  no visible difference reported).

### Confirmed present in the workspace (handover audit)

- `GlassMaterial` — stable public value type with validated setters; does
  not expose HLSL / constant-buffer / pass internals.
- `GlassSurface` — public rendering surface abstraction.
  - Owns `ID3D11Device` via `ComPtr` (extends device lifetime).
  - Borrows `ID3D11DeviceContext*` / backbuffer RTV / background SRV only
    for the duration of `Render()`.
  - Non-copyable, movable, RAII cleanup in destructor, `Reset()` idempotent.
- `SurfaceDesc` — exposes only `width` / `height`; `shaderDir` is internal
  and no longer part of the public descriptor.
- `Status` / `ErrorCode` — unified error model; no swallowed HRESULT, no
  magic bool.
- `DiagnosticStages` — kept separate from `GlassMaterial`.
- `samples/p1_smoke` — runtime/API smoke test using the P1 public API.
- `samples/p1_proof` — P1 public API visual parity sample.
- CMake builds `AuroraGlassCore` (static lib) + `P0_Proof` + `P1_Smoke` +
  `P1_Proof`.

### Evidence actually observed during the handover audit

Build:

```
cmake --build build --config Debug
  -> AuroraGlassCore.lib
  -> P0_Proof.exe
  -> P1_Proof.exe
  -> P1_Smoke.exe
```

Runtime (automated, 40 frames each, D3D11 debugLayer=yes):

```
P0_Proof.exe --frames 40   -> frames=41, ~183.8 FPS, 5.44 ms, clean exit
P1_Proof.exe --frames 40   -> frames=41, ~183.5 FPS, 5.45 ms, clean exit
P1_Smoke.exe --frames 40   -> frames=41, ~183.2 FPS, 5.46 ms, clean exit
```

### P1 Exit Criteria status (per ROADMAP.md)

Per the Owner ruling, ROADMAP / PHASE_GATE take precedence over
`CURRENT_STATE.md`. As of this update both previously-open ROADMAP P1
requirements have been implemented and verified within P1 scope:

1. **device-lost / recreate path** — DONE (minimal).
2. **material validation / resource lifetime tests** — DONE (`tests/p1_tests.cpp`).

#### 1. Minimal device-lost / recreate contract (implemented)

- `ErrorCode::DeviceLost` added; `Status::DeviceLost(hr)` preserves the HRESULT.
- `IsDeviceLostHResult(hr)` classifies `DXGI_ERROR_DEVICE_REMOVED` and
  `DXGI_ERROR_DEVICE_RESET`.
- `GlassSurface::CheckDeviceLost()` reports `DeviceLost` / `DeviceError` /
  `NotInitialized` / `Ok` based on `ID3D11Device::GetDeviceRemovedReason()`.
- `GlassSurface::Render()` and `Resize()` fail fast with `DeviceLost` if the
  device is already gone; `Create()` rejects an already-lost device.
- `GlassSurface::CreateShadersAndResources()` / `CreateBlurTargets()` map
  device-lost HRESULTs to `DeviceLost` (HRESULT preserved).
- `D3D11Device::Present()` now returns the HRESULT unmodified (no longer
  swallowed) and surfaces device removal; `D3D11Device::IsDeviceLost()` added.
- Recovery model: host owns device creation. On loss the host calls
  `GlassSurface::Reset()` (frees invalid GPU resources), creates a new
  `ID3D11Device`, then `GlassSurface::Create()` again. Core does NOT
  auto-recreate the device, spawn threads, retry, or run a recovery manager.

Note: D3D11 exposes no programmatic device-removal API (no D3D11 equivalent of
`ID3D12Device::RemoveDevice`), so the tests verify the observable contract and
the Reset→Create→Render recovery path. Real hardware-removal stress testing is
deferred to P2.

#### 2. Automated tests (implemented)

`tests/p1_tests.cpp` — minimal, no external framework, wired into CTest
(`add_test(NAME P1_Tests ...)`). Covers:

- A. GlassMaterial validation: legal range, boundaries, NaN, ±Inf, out-of-range
  clamping, rejected setter leaves prior value unchanged, defaults valid.
- B. Status / error semantics: Ok / InvalidArgument / DeviceError /
  ResourceError / ShaderError / DeviceLost, HRESULT preservation,
  `ErrorCodeToString`, `IsDeviceLostHResult`.
- C. GlassSurface lifecycle: default/uninitialized, Create, Reset, Reset x2
  idempotent, move construction, move assignment, moved-from safety,
  Create-after-Reset.
- D. Minimal device-lost / recreate contract + Reset→Create→Render recovery
  with a fresh device.

#### Verification actually executed (this update)

```
cmake --build build --config Debug
  -> AuroraGlassCore.lib, P0_Proof.exe, P1_Proof.exe, P1_Smoke.exe, P1_Tests.exe

ctest --test-dir build -C Debug
  -> 1/1 Test #1: P1_Tests ... Passed   (95 checks, 0 failures)

P0_Proof.exe --frames 40   -> clean exit, ~182.7 FPS
P1_Proof.exe --frames 40   -> clean exit, ~182.1 FPS (resize path exercised)
P1_Smoke.exe --frames 40   -> clean exit, ~183.1 FPS (resize path exercised)
```

P0 frozen baseline (`samples/p0_proof/main.cpp`) was NOT modified; its source
mtime (2026-09-22 12:09:34) predates all P1 work.

Full device-loss stress testing, multi-monitor/DPI, HDR and deep profiling
remain P2 and were intentionally not started.

### P1 non-goals (unchanged)

Controls, WPF / WinUI 3 adapters, cross-platform, layout/text/MVVM,
capture, D3D12/Vulkan/OpenGL/WebGPU, Rust bindings, premature abstractions.

---

## Owner Final Approval

Owner completed the P1 final review and approved P1 on 2026-09-22.

- GlassMaterial stable value API: PASS
- GlassSurface move-only / RAII: PASS
- Device ownership (ComPtr held internally): PASS
- Context / RTV / SRV borrowed per render call: PASS
- SurfaceDesc does not expose shaderDir: PASS
- DiagnosticStages separated from GlassMaterial: PASS
- Status / HRESULT error model: PASS
- Runtime smoke: PASS
- P1 Proof: PASS
- Owner Visual Parity: PASS
- Device-lost / recreate minimal contract: PASS
- P1 automated tests: 95 checks / 0 failures
- P0 Golden Baseline: not modified
- ROADMAP P1 Exit Criteria: all satisfied

**P1 = APPROVED / FROZEN.**

The P1 Core public API is now a frozen baseline. Future breaking changes to it
require an explicit, documented reason (no silent modifications).

## Completion Rule

P1 is frozen. Next phase is P2 — Core Stabilization, which has NOT been started.
The Owner instructs when P2 begins; the agent does not self-advance.
