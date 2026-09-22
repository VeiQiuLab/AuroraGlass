# AuroraGlass Core — Public API Review (P2 Slice 5)

**Candidate:** `v0.1-stable-candidate`
**Scope reviewed:** the frozen P1 public API surface.

`v0.1-stable-candidate` means:
- the API has completed the P2 review;
- future changes should default to avoiding breaking changes;
- it is **not** 1.0;
- it is **not** a cross-platform ABI guarantee;
- it does **not** mean P2 is finally approved (manual gates below remain).

---

## Reviewed surface

| Type | File |
|---|---|
| `GlassMaterial` | `src/core/glass_material.h` |
| `DiagnosticStages` | `src/core/glass_material.h` |
| `GlassSurface` | `src/core/glass_surface.h` |
| `SurfaceDesc`, `FrameInfo` | `src/core/glass_surface.h` |
| `Status`, `ErrorCode`, `IsDeviceLostHResult`, `ErrorCodeToString` | `src/core/result.h` |

## GlassMaterial

- **Ownership:** pure value type; owns no GPU resources.
- **Lifetime:** trivially copyable; no external lifetime coupling.
- **Threading:** may be constructed/modified on any thread (no GPU state).
- **Error semantics:** every setter returns `Status`; NaN/Inf → `InvalidArgument`;
  out-of-range is clamped (Ok), not rejected. Rejected setter leaves the prior
  value unchanged.
- **Units:** blurRadius/cornerRadius in pixels; the rest are normalized [0..n]
  factors (documented per setter).
- **Nullability:** none (no pointers).
- **Move/copy:** copyable value semantics; default-constructible (P1 defaults).
- **Ranges:** documented per setter (`[0,24]`, `[0,1]`, `[0,2]`, `[0,0.2]`,
  `[0,200]`, highlight `[-1,1]`).
- **Implementation leakage:** none — no HLSL / constant-buffer / pass exposure.
- **Blocking flaw:** none found.

## DiagnosticStages

- **Ownership/lifetime:** plain value struct; no resources.
- **Purpose:** diagnostic toggles, **separate from material semantics**.
- **Threading:** value type; consumed by `Render` on the render thread.
- **Move/copy:** copyable; `AllEnabled()` / `AllDisabled()` helpers.
- **Implementation leakage:** none (it is a public diagnostic contract).
- **Blocking flaw:** none. It is intentionally not part of `GlassMaterial`.

## GlassSurface

- **Ownership:** **owns** `ID3D11Device` via `ComPtr` (extends device lifetime).
- **Borrowing:** `ID3D11DeviceContext*`, backbuffer RTV, and background SRV are
  **borrowed per `Render()` call only**.
- **Lifetime / RAII:** destructor deterministically frees all owned GPU
  resources; `Reset()` is idempotent and safe after any failure.
- **Threading:** single render thread contract; no locks/pools/async. All methods
  must run on the thread owning the borrowed context.
- **Error semantics:** all fallible ops return `Status`. `SetMaterial` is a
  CPU-only value copy and returns `void` (cannot fail for a validated material).
- **Units:** `SurfaceDesc::width/height` and `Width()/Height()` are **physical
  pixels**. DPI is a host concern and never enters Core.
- **Nullability:** `Create` rejects a null device; `Render` rejects null
  context/RTV/SRV with `InvalidArgument`.
- **Move/copy:** **non-copyable**, **movable**; moved-from objects are empty and
  safe (Reset/resize report `NotInitialized`).
- **Device-lost:** `CheckDeviceLost()` reports `DeviceLost`; `Render`/`Resize`
  fail fast with `DeviceLost`; `Create` rejects an already-lost device.
- **Create failure cleanup:** on any creation failure the object is `Reset()`
  (no half-initialized state).
- **Implementation leakage:** none — shader discovery and pass order are internal;
  `SurfaceDesc` exposes only width/height.
- **Blocking flaw:** none found.

## SurfaceDesc / FrameInfo

- `SurfaceDesc`: `{width,height}` only; **physical pixels**; no shader path.
- `FrameInfo`: `{timeSeconds, DiagnosticStages}`; per-frame input only.
- Both are plain structs; no ownership or lifetime concerns.
- **Blocking flaw:** none.

## Status / ErrorCode

- **Ownership/lifetime:** value type.
- **Error semantics:** `Ok`, `InvalidArgument`, `DeviceError`, `ResourceError`,
  `ShaderError`, `NotInitialized`, `DeviceLost`; `HRESULT` preserved in `hr`.
- `IsDeviceLostHResult(hr)` classifies removed/reset.
- **Blocking flaw:** none.

---

## Error-path fixes made in this slice (non-frozen internals only)

- `D3D11Device::Init` no longer swallows the internal `Resize` result; on failure
  it releases all resources and returns `false` (no half-initialized object).
- `BackgroundSource::Init`/`Resize` no longer leave half-initialized or stale
  resources; on failure the object is released and its reported size is reset to
  0 so `TextureSRV()/Width()/Height()` stay consistent with actual GPU resources.
- `BackgroundSource::Render` no longer silently continues on a failed constant-
  buffer `Map`; it skips the draw and emits a debug message (no HRESULT return
  path added, to avoid disturbing the frozen P0 baseline).

These are host/support classes, **not** the frozen P1 public API. No frozen
public signature changed.

## Not automatically covered (honest gaps)

- **Real device removal / reset** — D3D11 has no programmatic removal API; only
  the classification helper + recovery path are tested. Real removal remains a
  manual/debug item.
- **Forced shader-compile failure on a healthy device** — not reliably triggerable
  without corrupting environment; not faked.
- **Multi-monitor DPI switching** — manual (see docs/P2_DPI_VALIDATION.md).
- **Zero live GPU objects** — requires manual/debugger inspection of
  `ReportLiveDeviceObjects` output (DebugView/VS Output).

## Conclusion

No blocking design flaw was found in the frozen P1 public API. The interface is
kept **unchanged**. The Core public API may be recorded as
**`v0.1-stable-candidate`**, pending the manual P2 gate items below.
