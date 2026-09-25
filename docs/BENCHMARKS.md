# AuroraGlass Benchmarks

This document records what was actually measured for AuroraGlass, how it was
measured, on what environment, and — importantly — what the numbers do NOT
prove. It is not a marketing benchmark.

## Purpose

Give an SDK consumer an honest, reproducible picture of the CPU and rendering
cost of small operations that AuroraGlass exposes. Two kinds of measurement are
recorded:

1. CPU micro-benchmarks — cost of individual CPU-only operations an app runs
   every frame (material setters, motion stepping, control input).
2. Core rendering path — the cost of submitting one glass frame through the
   public GlassSurface API, measured offscreen.

Nothing here is a frame-rate guarantee. There is no FPS claim for a real
on-screen application, because no benchmark in this repository measures a real
present/vsync loop.

## Environment

Measurements were taken on a single machine. These are not a general hardware
performance claim.

| Item | Value |
| --- | --- |
| OS | Windows 11 (build 10.0.26200) |
| CPU | AMD Ryzen 9 7940HX |
| GPU | AMD Radeon RX 7600 XT |
| GPU driver | 32.0.31041.1004 |
| Display mode at test time | 2560x1440 |
| Build toolchain | MSVC (Visual Studio 17 2022), CMake 4.4.3 |
| SDK version | 0.8.1 |
| Runtime contract | native HWND DPI; Debug validated, Release NOT TESTED as an SDK claim |

## Methodology

### CPU micro-benchmarks

Harness: tests/p8_benchmark/main.cpp, target P8_Cpu_Benchmark.

- Timing: QueryPerformanceCounter around a batch of N iterations.
- Per-iteration value: batch_ns / N.
- Warm-up: one discarded batch before timing.
- Trials: 15 batches of 200,000 iterations each.
- Reported: MEDIAN and P95 of the 15 per-trial values (robust to outliers).
- CPU-only: no GPU, no HWND, no rendering. Uses only the frozen public headers.

### Core rendering path

Harness: samples/p2_bench/main.cpp, target P2_Bench (pre-existing P2 baseline).

- Fixed-size offscreen D3D11 render target (no swap chain).
- CPU: QueryPerformanceCounter around the GlassSurface::Render() submission
  call only — host-side submission time, not a frame interval.
- GPU: D3D11 timestamp queries (TIMESTAMP_DISJOINT + start/end TIMESTAMP),
  resolved per frame; GPU ms = (end - start) / Frequency * 1000. Disjoint
  frames are excluded.
- Warm-up: 300 frames (discarded). Measured: 1000 frames.
- Explicitly excluded: Present / vsync, and background generation (a static
  background SRV is used).

## Benchmarks

### B1. CPU micro-benchmarks (Release)

Command:

    cmake --build build --config Release --target P8_Cpu_Benchmark
    build/Release/P8_Cpu_Benchmark.exe

| Operation | median ns/op | p95 ns/op |
| --- | ---: | ---: |
| material.SetBlurRadius | 2.99 | 3.13 |
| material.snapshot(all getters) | 1.50 | 1.52 |
| motion.Tween1D.Step | 1.79 | 1.81 |
| motion.Spring1D.Step | 5.74 | 5.75 |
| controls.Button.PressRelease | 7.46 | 7.50 |
| controls.Slider.DragMove | 13.54 | 13.77 |

(iters=200000, trials=15 per operation)

### B1-D. CPU micro-benchmarks (Debug, correctness/reference only)

These are shown only to document the expected Debug-vs-Release relationship.
Debug is NOT the product performance baseline.

| Operation | median ns/op | p95 ns/op |
| --- | ---: | ---: |
| material.SetBlurRadius | 19.47 | 19.69 |
| material.snapshot(all getters) | 27.32 | 27.43 |
| motion.Tween1D.Step | 3.48 | 3.52 |
| motion.Spring1D.Step | 28.02 | 28.19 |
| controls.Button.PressRelease | 36.90 | 36.98 |
| controls.Slider.DragMove | 44.72 | 44.87 |

### B2. Core rendering path (Release, debug layer OFF)

Command:

    cmake --build build --config Release --target P2_Bench
    build/Release/P2_Bench.exe --no-debug --warmup 300 --frames 1000

| Resolution | cpu_avg_ms | gpu_avg_ms | gpu_min_ms | gpu_max_ms | disjoint_frames |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 0.0026 | 0.2030 | 0.1899 | 0.5615 | 0 |
| 2560x1440 | 0.0026 | 0.3349 | 0.3115 | 0.7852 | 0 |

GPU-equivalent throughput (1000 / gpu_avg_ms): 4924.99 (1080p), 2985.79 (1440p).
This is isolated GPU render throughput, NOT a real application FPS. See
Interpretation.

## Results

- CPU micro-operations are on the order of a few nanoseconds per call in
  Release (2-14 ns). None of them is a plausible per-frame bottleneck on its
  own.
- The Core glass pipeline submits in a few microseconds of CPU time per frame
  (0.0026 ms) and renders in roughly 0.2-0.33 ms of isolated GPU time for the
  measured resolutions.

## Interpretation

- CPU micro-benchmarks measure the cost of individual pure-CPU calls. They do
  not include rendering, layout, or input routing.
- The rendering numbers isolate the Core glass pipeline on a fixed offscreen
  target. They exclude Present/vsync, window/composition cost, and background
  generation.
- The '60 FPS judgment' in the P2 baseline compares isolated GPU render time
  (0.2030 ms / 0.3349 ms) against the 16.67 ms 60 FPS budget. Both pass. This
  is a statement about Core render cost headroom, NOT a claim that any
  application runs at 60 FPS.

## Stability / stress validation (not a throughput benchmark)

The P2 stress test validates stability under repeated resize / error-path
workloads:

- Test: P2_Stress_Tests (CTest).
- Typical execution duration on this machine: ~64.7 s (observed range ~64.7 s).
- Result: PASS.

This is a regression / stability proof. It is NOT a rendering throughput
benchmark and must not be read as one.

## Limitations

- Single machine, single GPU; not a general performance claim.
- No real on-screen FPS was measured. Present/vsync, window composition, and
  background generation are excluded, so real on-screen frame rate is bounded
  by the host present path, not by Core render cost.
- GPU timing depends on driver timestamp support; values are consistent but
  should be treated as relative indicators.
- Debug numbers are not product performance. Release is used for the CPU
  micro-benchmarks; the Core rendering path numbers are from the pre-existing
  P2 Release baseline.
- No comparison against any other framework is made or implied.
- Numbers are a starting reference, not a guarantee.

## How to reproduce

CPU micro-benchmarks (Release):

    cmake -S . -B build
    cmake --build build --config Release --target P8_Cpu_Benchmark
    build/Release/P8_Cpu_Benchmark.exe

CPU micro-benchmarks (Debug):

    cmake --build build --config Debug --target P8_Cpu_Benchmark
    build/Debug/P8_Cpu_Benchmark.exe

Core rendering path (Release):

    cmake --build build --config Release --target P2_Bench
    build/Release/P2_Bench.exe --no-debug --warmup 300 --frames 1000

Stability / stress:

    ctest --test-dir build -C Debug -R P2_Stress_Tests --output-on-failure
