# Upgrading AuroraGlass

This guide tells an SDK consumer what to check when moving from one
AuroraGlass SDK version to another: what may break, what usually does not,
and how to refresh a staged SDK safely. It is not a changelog.

## Current version status

- SDK version: **0.8.0**
- Status: **pre-1.0**
- Version source: the repository root `VERSION` file.

AuroraGlass is **not** declared 1.0-stable. P8 establishes the first formally
versioned, distributable SDK baseline, but it has not been frozen yet. See
`docs/VERSIONING.md` for the versioning policy.

## Version source

`VERSION` is the canonical version source. All derived version outputs
(native version API, CMake package version, managed assembly metadata, release
artifact naming) derive from it.

A consumer should NOT:

- guess the version from a DLL file name;
- treat a commit hash as the SDK version;
- hard-code its own AuroraGlass version constant.

Read the version from the staged `VERSION` file (and the generated
`auroraglass/version.h` / managed `ASSEMBLY_VERSION`).

## What to check when upgrading

Checklist:

- [ ] read the version / release notes
- [ ] compare the public API baseline (`api/*.txt`) for drift
- [ ] rebuild the native consumer (re-run CMake configure)
- [ ] refresh the installed SDK stage (`cmake --install`)
- [ ] replace the native runtime DLLs
- [ ] replace the managed assemblies
- [ ] replace shaders / runtime resources
- [ ] confirm WPF / WinUI target compatibility
- [ ] rerun an integration smoke test

This checklist follows the current staged package layout: `VERSION`, `include/`,
`lib/`, `bin/`, `managed/`, `shaders/`.

## Native consumers

When upgrading the native SDK, the artifacts that matter are:

- installed public headers (`include/`, derived from
  `api/native_public_headers.txt` plus `auroraglass/version.h`)
- import libraries (`lib/<Config>/`)
- runtime DLLs (`bin/<Config>/`)
- CMake package files (`lib/cmake/AuroraGlass/`)
- shaders (`shaders/`)

Recommendations:

- re-run `cmake` configure so the imported targets are refreshed;
- do NOT mix old headers with new libraries;
- do NOT mix Debug and Release artifacts.

## WPF

Keep the following from the **same** SDK version:

- `AuroraGlass.Wpf.dll` (`managed/WPF/<Config>/`)
- `AuroraGlassWpfInterop.dll` (`bin/<Config>/`)
- shaders / runtime resources

Do NOT replace only the managed DLL while keeping an old native interop DLL.
The managed assembly and its native bridge are a matched pair.

## WinUI 3

Keep the following from the **same** SDK version:

- `AuroraGlass.WinUI.dll` (`managed/WinUI/<Config>/`)
- `AuroraGlassWinUIInterop.dll` (`bin/<Config>/`)
- runtime resources

Also re-check:

- Windows App SDK compatibility (the validated path uses
  `Microsoft.WindowsAppSDK 2.5.1`)
- target framework (`net10.0-windows10.0.19041.0`)
- RuntimeIdentifier / package model (validated: `win-x64`,
  `WindowsPackageType=None`)

## Public API baseline

P8 established baseline manifests:

    api/native_public_headers.txt
    api/wpf_public_api.txt
    api/winui_public_api.txt
    api/wpf_interop_exports.txt
    api/winui_interop_exports.txt

These exist to detect public surface drift. A consumer does not need to depend
on these files to run a program, but they are the basis of the SDK
compatibility contract. When the baseline changes, that change is a versioning
event (see `docs/VERSIONING.md`).

## Pre-1.0 warning

Because the current version is 0.8.0, the pre-1.0 policy applies:

> 0.x releases may still contain intentional breaking API changes, but such
> changes must follow the repository's documented SemVer/pre-1.0 policy and
> require explicit upgrade notes.

This is deliberately not 'anything can break at any time'. AuroraGlass already
has a frozen public baseline, and breaking changes are constrained (MINOR-only,
never silent PATCH).

## Upgrading to 0.8.0

0.8.0 is the first formally versioned / distributable SDK baseline established
by P8. No earlier distributable SDK upgrade path is claimed. If you have code
that used the repository sources directly before P8, treat this as an initial
integration against a staged SDK rather than a version-to-version upgrade.
