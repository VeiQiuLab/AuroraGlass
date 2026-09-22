# Current Project State

## Active Phase

**P2 — Core Stabilization**

Gate state: `APPROVED / FROZEN`

Last updated: 2026-09-22

**Core Public API Candidate:** `v0.1-stable-candidate`

**Next Phase:** P3 — Controls (NOT started; awaiting Owner instruction).

## Previous Phases

**P1 — Core Library**: `APPROVED / FROZEN`
- Owner Visual Parity: PASS
- Frozen baselines retained: `samples/p0_proof`, `samples/p1_smoke`, `samples/p1_proof`, `tests/p1_tests`

**P0 — Visual Core Proof**: `APPROVED / FROZEN`
- Owner Visual Review: PASS
- Demo retained as frozen golden baseline (`samples/p0_proof`), never to be modified

## Frozen Baselines (do not delete or modify)

| Artifact | Role |
|----------|------|
| `samples/p0_proof` | Frozen P0 Golden Baseline (DO NOT MODIFY, DELETE, OR REWRITE) |
| `samples/p1_smoke` | P1 API / runtime verification |
| `samples/p1_proof` | P1 public API visual reference |
| `tests/p1_tests` | P1 automated tests (95 checks) |

The P1 Core public API is a frozen baseline, now recorded as
`v0.1-stable-candidate`. Breaking changes to it require an explicit, documented
reason; no silent modifications.

## Current Goal (P2 — Core Stabilization)

Make Core dependable before building UI wrappers: DPI/multi-monitor, resize/
minimize/restore, performance baselines, lifecycle stress, resource-leak checks,
parameter boundary tests, and an API review/freeze candidate.

## P2 Required Capabilities (ROADMAP) — status

1. DPI handling — DONE (host-side; Core stays pixel-only)
2. multi-monitor validation — DONE (Owner Manual Validation = PASS)
3. resize/minimize/restore validation — DONE
4. performance baselines at 1080p and 1440p — DONE
5. repeated create/destroy stress test — DONE
6. GPU/CPU resource leak checks — DONE (no application-owned D3D11 resource leak observed)
7. parameter boundary tests — DONE
8. API review/freeze candidate — DONE (`v0.1-stable-candidate`)

## P2 Explicit Non-Goals

Do NOT implement in P2: Controls, GlassButton/Panel/Card/Slider/Toggle, WPF /
WinUI 3 / Qt adapters, cross-platform, layout/text/MVVM, new visual effects,
plugin/theme systems, AI, installer/updater, HDR, HDR/benchmark-driven renderer
rewrites.

## P2 Deliverables (frozen)

| Artifact | Role |
|----------|------|
| `samples/p2_resize_smoke` | Resize / minimize / restore diagnostic host |
| `samples/p2_dpi_smoke` | Per-monitor DPI diagnostic host |
| `samples/p2_bench` | Core rendering performance baseline (offscreen) |
| `tests/p2_stress_tests` | Repeated lifecycle stress |
| `tests/p2_resize_tests` | Resize / zero-size robustness |
| `tests/p2_errorpath_tests` | Error-path coverage |
| `tests/liveobject_capture.cpp` | DebugView-style live-object capture tool |
| `docs/P2_DPI_VALIDATION.md` | DPI pixel contract + manual validation path |
| `docs/P2_PERFORMANCE_BASELINE.md` | 1080p / 1440p baseline |
| `docs/P2_API_REVIEW.md` | Core public API review (`v0.1-stable-candidate`) |

## Completion Rule

P2 is APPROVED / FROZEN. Next phase is P3 — Controls, NOT started. Wait for Owner
instruction before beginning P3. The agent does not self-advance.
