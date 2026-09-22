# Current Project State

## Active Phase

**P1 — Core Library**

Gate state: `APPROVED / FROZEN`

Last updated: 2026-09-22

**Next Phase:** P2 — Core Stabilization (NOT started; awaiting Owner instruction).

## Previous Phase

**P0 — Visual Core Proof**: `APPROVED / FROZEN`
- Owner Visual Review: PASS
- Demo retained as frozen golden baseline (`samples/p0_proof`), never to be modified

## Frozen Baselines (do not delete)

| Artifact | Role |
|----------|------|
| `samples/p0_proof` | Frozen P0 Golden Baseline (DO NOT MODIFY, DELETE, OR REWRITE) |
| `samples/p1_smoke` | P1 API / runtime verification |
| `samples/p1_proof` | P1 public API visual reference |
| `tests/p1_tests` | P1 automated tests (95 checks) |

The P1 Core public API is now a frozen baseline. Breaking changes to it require
an explicit, documented reason; no silent modifications.

## Current Goal (P1)

Refactor the P0-verified liquid-glass rendering core into a reusable Core Library
with stable public API, clear lifecycle, and documented ownership/threading contract.

## P1 Required Capabilities

1. GlassMaterial — stable public material model (covers P0 parameters, hides HLSL internals)
2. GlassSurface — reusable rendering surface abstraction (hides pass order, intermediate RTs, CB layout)
3. Ownership / Lifecycle — Device held via ComPtr; ctx/RTV/SRV borrowed per call; deterministic Reset(); RAII
4. Error Model — unified Status enum, no swallowed HRESULT, no magic bool
5. Threading Contract — single render thread contract (no locks/pools/async)
6. Background Input Boundary — minimal Core contract (ID3D11ShaderResourceView*); BackgroundSource at sample layer
7. Shader/Resource Internals — internal; not exposed in public API; stage toggles preserved for diagnosis
8. P1 Compatibility Sample — p1_proof uses new P1 API; visual parity with p0_proof verified

## P1 Explicit Non-Goals

Do NOT implement:

- GlassButton / GlassPanel / GlassCard / GlassSlider / GlassToggle
- Controls hierarchy
- WPF / WinUI 3 / Qt adapters
- Cross-platform rendering
- Layout engine / text system / MVVM / navigation
- Theme system / plugin system / AI / updater / installer
- Desktop Duplication / Screen Capture
- D3D12 / Vulkan / OpenGL / WebGPU rewrites
- Rust bindings
- Premature abstractions "for later"

## Sample Responsibility Matrix (Frozen)

| Sample | Role | Modification Policy |
|--------|------|---------------------|
| samples/p0_proof | **Frozen Golden Baseline** | **DO NOT MODIFY, DELETE, OR REWRITE. Ever.** |
| samples/p1_smoke | API / Runtime verification | May be updated alongside P1 API |
| samples/p1_proof | P1 Public API visual parity | May be updated alongside P1 API |

## Completion Rule

Owner has confirmed visual parity and approved P1.

1. Gate state marked `APPROVED / FROZEN` (done 2026-09-22).
2. Next Phase = P2 — Core Stabilization, NOT started. Wait for Owner instruction
   before beginning P2.
