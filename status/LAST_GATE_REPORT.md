# AuroraGlass — Gate Report (Last Gate)

**Last updated:** 2026-09-22
**Active Phase:** P2 — Core Stabilization
**P2 Gate Result:** APPROVED / FROZEN (Owner Final P2 Review 2026-09-22)
**Core Public API Candidate:** `v0.1-stable-candidate`
**Previous Phases:** P0 APPROVED / FROZEN; P1 APPROVED / FROZEN
**Next Phase:** P3 — Controls (NOT started)

> This report records only validations that were actually executed and
> observed. No un-executed verification is recorded as passing.

---

## P2 — Core Stabilization: APPROVED / FROZEN

### Owner Manual Gates

- **Owner Multi-monitor / DPI Manual Validation = PASS** — Owner moved the
  window back and forth across different DPI / monitor paths with no crash and
  no obvious visual corruption.
- **D3D11 Live Object Manual Gate = PASS** — Owner captured the original
  DebugView report; the follow-up lifecycle diagnosis confirmed:
  - every non-Device reported object had Refcount 0 / IntRef 1;
  - with `D3D11_RLDO_IGNORE_INTERNAL` those runtime-internal objects disappear;
  - no AuroraGlass GPU resource still held an external COM reference;
  - the single remaining `Live ID3D11Device` is required to keep the
    `ID3D11Debug` alive for the report.

Accurate conclusion recorded (per Owner): **"No application-owned D3D11 resource
leak was observed."** (This is NOT a claim that "absolutely zero D3D objects
exist at report time".)

### Implementation slices (all PASS)

- **Slice 1 — Repeated lifecycle stress**: `tests/p2_stress_tests` (9800 checks).
  Working set converges (round-1 +25048 KB, round-2 marginal +616 KB) → no
  sustained-growth leak. Debug Layer ACTIVE. `ReportLiveDeviceObjects` executed.
- **Slice 2 — Resize / minimize / restore robustness**: hardened
  `D3D11Device::Resize` (HRESULT not swallowed; device-lost classified),
  transactional `GlassSurface::Resize`, SRV unbind hygiene in
  `GlassSurface::Render`. New `samples/p2_resize_smoke` +
  `tests/p2_resize_tests` (48 checks).
- **Slice 3 — DPI / multi-monitor validation**: host enables
  `PER_MONITOR_AWARE_V2` before HWND creation, handles `WM_DPICHANGED` via the
  suggested RECT, drives resize from real client pixels. DPI never enters Core.
  New `samples/p2_dpi_smoke` + `docs/P2_DPI_VALIDATION.md`.
- **Slice 4 — Performance baseline**: offscreen benchmark via the P1 public API,
  CPU (QPC around `Render`) and GPU (D3D11 timestamp queries) separated.
  New `samples/p2_bench` + `docs/P2_PERFORMANCE_BASELINE.md`.
- **Slice 5 — Error-path coverage + API review**: hardened
  `D3D11Device::Init` and `BackgroundSource` error paths; new
  `tests/p2_errorpath_tests` (34 checks); `docs/P2_API_REVIEW.md` records the
  Core public API as `v0.1-stable-candidate` with no blocking design flaw.
- **Live-object diagnosis**: added `tests/liveobject_capture.cpp` (DebugView-
  style capture) and documented `D3D11Device::ReportLiveObjects` default flags
  (`DETAIL | IGNORE_INTERNAL`) with rationale.

### Performance baseline (Release, debug layer OFF; single machine)

GPU time via D3D11 timestamp queries; Present/vsync and background generation
excluded (offscreen, static SRV). FPS-like values are **GPU-equivalent
throughput based on isolated GPU render time, NOT real app FPS**.

- 1920x1080: gpu_avg 0.1988 ms, cpu_avg 0.0025 ms → PASS (16.67 ms threshold)
- 2560x1440: gpu_avg 0.3311 ms, cpu_avg 0.0025 ms → PASS

### CTest results

```
1/4 P1_Tests            Passed
2/4 P2_Stress_Tests     Passed
3/4 P2_Resize_Tests     Passed
4/4 P2_ErrorPath_Tests  Passed
100% tests passed out of 4
```

### API

`docs/P2_API_REVIEW.md` — no blocking flaw found; the frozen P1 public API is
kept unchanged and recorded as **`v0.1-stable-candidate`** (`v0.1-stable-candidate`
means P2 review complete and future changes default to non-breaking; it is NOT
1.0, NOT a cross-platform ABI guarantee, and NOT a final approval of a later
phase).

### Known limitations (honest)

- D3D11 has no programmatic device-removal API; real hardware removal was not
  forced (only the classification helper + recovery path are tested).
- Performance numbers are single-machine (Ryzen 9 7940HX / Radeon RX 7600 XT),
  offscreen, and exclude Present/composition; not a general hardware claim.
- "No application-owned D3D11 resource leak was observed" is scoped to the
  report-time evidence above; it is not an absolute zero-object claim.
- Forced shader-compile failure on a healthy device is not reliably triggerable
  and is not faked.

## Completion Rule

P2 is frozen. Next phase is P3 — Controls, which has NOT been started. The Owner
instructs when P3 begins; the agent does not self-advance.
