# AuroraGlass Versioning Rules

This document defines how AuroraGlass SDK versions are chosen. It is based on
Semantic Versioning 2.0.0 (MAJOR.MINOR.PATCH) but is written against the actual
AuroraGlass public API / ABI and package layout, not as a copy of the SemVer
spec.

## 1. Version authority

The root `VERSION` file is the canonical AuroraGlass SDK semantic version.

All of the following MUST derive from `VERSION` (no second hand-written version
source anywhere):

- the native version API (generated `auroraglass/version.h`)
- the CMake package version
- managed assembly / package metadata
- release artifact naming

Current value: `0.8.0`.

## 2. What counts as public contract

The public contract is classified as:

- installed native public headers (installed tree derived from
  `api/native_public_headers.txt` plus `auroraglass/version.h`)
- public native types / functions
- installed CMake targets (`AuroraGlass::*`)
- WPF public managed API (`api/wpf_public_api.txt`)
- WinUI public managed API (`api/winui_public_api.txt`)
- WPF native C ABI exports (`api/wpf_interop_exports.txt`)
- WinUI native C ABI exports (`api/winui_interop_exports.txt`)
- the documented runtime / deployment contract (shader layout, native bridge
  DLL names, managed assembly names)

The following are internal and NOT part of the public contract:

- `src/` internal headers
- tests
- sample implementation details
- private helpers
- benchmark harnesses
- the build directory layout

## 3. PATCH change (0.8.0 -> 0.8.1)

Allowed:

- bug fixes
- documentation fixes
- performance improvements with no public semantic change
- packaging fixes
- runtime correctness fixes that preserve public behavior

NOT allowed in a PATCH:

- removing public API
- renaming public API
- changing a function signature
- changing documented semantics incompatibly

A PATCH MUST remain backward-compatible.

## 4. MINOR change (0.8.x -> 0.9.0)

MINOR may add backward-compatible public API.

Under the current pre-1.0 phase, intentional breaking changes are also
permitted **only at a MINOR boundary**, never silently in a PATCH, and MUST
include:

- an API baseline update
- explicit upgrade notes (see `docs/UPGRADING.md`)
- a compatibility review
- a full regression run

This keeps 0.x evolution possible without allowing '0.x breaks at any time'.

## 5. MAJOR change (1.x -> 2.0.0)

MAJOR denotes a formally breaking public contract change. Examples:

- remove / rename a public type
- incompatible signature change
- ABI-breaking export change
- incompatible package / layout contract change
- documented behavior semantic break

Before 1.0.0 the MAJOR number stays 0; the pre-1.0 policy in section 6 applies.

## 6. Pre-1.0 policy

Current version: `0.8.0`.

- PATCH: MUST remain backward-compatible.
- MINOR: may introduce intentional breaking public changes, but only with an
  API baseline update, upgrade notes, compatibility review, and full
  regression.

Breaking PATCH is NOT allowed. This is the key pre-1.0 rule.

## 7. ABI rules

Three surfaces are handled separately.

### C++ public API

Source compatibility is the primary concern. Binary compatibility across
arbitrary compiler / toolset versions is NOT guaranteed (see section 8).

### C ABI interop DLL

ABI-breaking changes include:

- removing an export
- changing a parameter layout
- changing struct size / layout incompatibly
- changing calling convention
- changing a numeric enum / status contract incompatibly

Adding a new export is normally backward-compatible, provided no existing
export is broken.

### Managed public API

See section 9.

## 8. C++ ABI limitation

AuroraGlass native C++ SDK targets the MSVC / Windows ecosystem. Binary
compatibility across arbitrary compiler / toolset versions is not guaranteed
unless explicitly validated in `docs/COMPATIBILITY.md`. AuroraGlass does NOT
promise ABI compatibility across all MSVC toolsets.

## 9. Managed API rules

For WPF / WinUI:

Breaking examples:

- remove a public class
- remove a public member
- change constructor requirements
- incompatible property type change
- change documented lifecycle semantics

Non-breaking examples:

- add a new class
- add an optional capability
- add a new method without changing the existing contract

Note potential breaking cases even when adding: overload ambiguity, or a new
overload that changes binding resolution.

## 10. Package contract

Once written into formal consumer docs, these are compatibility concerns and
must not be treated as fully internal:

- installed public headers
- CMake target names
- documented runtime DLL names
- managed assembly names
- required runtime resource layout (shaders)

Changing them is a versioning event at the appropriate level.

## 11. Deprecation policy

Minimal rule:

- after 1.0, public API should normally be deprecated before removal.
- pre-1.0 is more flexible, but a breaking removal still requires a MINOR bump
  plus upgrade notes.

No complex annotation framework is introduced.

## 12. Version bump checklist

For a maintainer preparing a release:

- [ ] determine PATCH / MINOR / MAJOR
- [ ] update the root `VERSION` only
- [ ] regenerate / validate derived version outputs
- [ ] run public API baseline checks
- [ ] update upgrade notes if breaking
- [ ] update compatibility docs if requirements changed
- [ ] run full build / tests
- [ ] validate external consumers
