# P2 — Core Rendering Performance Baseline (Slice 4)

Status: measured. **No renderer optimization was performed in this slice**
(measure first, record first).

## Method

`samples/p2_bench` measures **AuroraGlass Core rendering** via the P1 public
`GlassSurface` API on a **fixed-size offscreen D3D11 render target**, so the
result does not depend on the current desktop/window size or on a swap chain.

- Warm-up: 300 frames (discarded)
- Measured: 1000 frames
- Resolutions: 1920x1080 and 2560x1440 (fixed, offscreen)
- Material: `GlassMaterial{}` defaults (P1-verified)
- Background: a single static `R8G8B8A8` SRV (no per-frame generation)
- All `DiagnosticStages` enabled

### Explicitly excluded from the measured region

- **Present / vsync** — none; offscreen render target, no swap chain.
- **Background generation** — a static background SRV is used.

## CPU vs GPU timing (separated)

- **CPU**: `QueryPerformanceCounter` immediately around the
  `GlassSurface::Render()` submission call only. This is host-side submission
  time, **not** GPU execution time, and **not** a frame interval.
- **GPU**: D3D11 timestamp queries per frame —
  `D3D11_QUERY_TIMESTAMP_DISJOINT`, a start `TIMESTAMP`, the render workload,
  an end `TIMESTAMP`. GPU ms = `(end - start) / Frequency * 1000`.
  - Query data is polled until ready (`GetData` loop on `S_FALSE`).
  - If `Disjoint == true`, that frame is **invalid and excluded**; the count of
    disjoint frames is reported (`disjoint_frames`). Zero were observed.
  - The FPS-like value shown = `1000 / gpu_avg_ms`. It is
    **GPU-equivalent throughput based on isolated GPU render time** — it is
    **not** a real window/application FPS and must never be read as one.
    It is never `1 / CPU submission time`, and it excludes Present/vsync,
    window/composition cost, and background generation.
- Present/vsync is **not** used as a result.

## Environment

- OS: Windows 11 (build 10.0.26200)
- CPU: AMD Ryzen 9 7940HX
- GPU: AMD Radeon RX 7600 XT (driver 32.0.31041.1004)
- Display mode at test time: 2560x1440

These are single-machine measurements; they are **not** a general hardware
performance claim.

## Builds

- Official baseline: **Release**, **D3D11 debug layer OFF**.
- A Debug / debug-layer-ON run is included only as a correctness check; it is
  **not** the performance baseline.

## Results — Release, debug layer OFF (official baseline)

### 1920x1080

| Metric | Value |
|---|---|
| cpu_avg_ms | 0.0025 |
| cpu_min_ms | 0.0014 |
| cpu_max_ms | 0.0321 |
| gpu_avg_ms | 0.1988 |
| gpu_min_ms | 0.1902 |
| gpu_max_ms | 0.2074 |
| gpu_equivalent_fps (1000/gpu_avg; isolated GPU render throughput, NOT real app FPS) | 5029.16 |
| disjoint_frames | 0 |

### 2560x1440

| Metric | Value |
|---|---|
| cpu_avg_ms | 0.0025 |
| cpu_min_ms | 0.0014 |
| cpu_max_ms | 0.0305 |
| gpu_avg_ms | 0.3311 |
| gpu_min_ms | 0.3176 |
| gpu_max_ms | 0.3668 |
| gpu_equivalent_fps (1000/gpu_avg; isolated GPU render throughput, NOT real app FPS) | 3020.66 |
| disjoint_frames | 0 |

## Results — Debug, debug layer ON (correctness check only)

Quick run (200 measured frames):

| Resolution | cpu_avg_ms | gpu_avg_ms |
|---|---|---|
| 1920x1080 | 0.0174 | 0.1860 |
| 2560x1440 | 0.0176 | 0.3313 |

Debug CPU submission time is ~7x higher than Release (expected); GPU time is
close to Release, as expected (debug layer mainly penalizes CPU-side validation).

## 60 FPS judgment (threshold = 16.67 ms/frame)

This judgment uses **gpu_avg_ms** (isolated GPU render time), i.e. whether the
Core glass pipeline can be rendered fast enough for 60 FPS headroom. It is a
Core-render criterion, not a real window/application FPS claim.
(Release, debug OFF):

- 1920x1080: **PASS** (0.1988 ms << 16.67 ms)
- 2560x1440: **PASS** (0.3311 ms << 16.67 ms)

## Known limitations

- Single machine, single GPU; not a general performance claim.
- Offscreen measurement excludes Present/vsync and window/composition cost, so
  real on-screen FPS is bounded by the host present path, not by Core render.
- Background generation is excluded (static SRV), so these numbers isolate the
  Core glass pipeline, not the full sample frame cost.
- GPU timing depends on the driver's timestamp support; values are consistent
  but should be treated as relative indicators.
- This baseline is a **starting reference**, not a guarantee.
