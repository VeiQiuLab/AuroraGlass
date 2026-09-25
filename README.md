# AuroraGlass

AuroraGlass is a **reusable Windows Liquid Glass UI SDK**.

It provides a high-quality liquid-glass rendering/material system that can be
embedded into multiple Windows applications without each application
reimplementing the effect. AuroraGlass is **not** a general-purpose UI framework.

## Platform and stack

- **Windows-only** (Windows 10/11, x64 first)
- **C++20**, **Direct3D 11**, HLSL
- First-class **Win32** integration
- **WPF** and **WinUI 3** adapters

## Current SDK version

`0.8.1`

The canonical version source is the root `VERSION` file. See
[Versioning Rules](docs/VERSIONING.md).

## Quick start

```text
cmake -S . -B build
cmake --build build --config Debug
cmake --install build --config Debug --prefix build/sdk-stage/AuroraGlass-0.8.1
```

Full build and external-consumption instructions:
[Build and Consume](docs/BUILD_AND_CONSUME.md).

## Documentation

- [docs/](docs/README.md) — documentation index
- [Build and Consume](docs/BUILD_AND_CONSUME.md) — staging the SDK and consuming it externally
- [Samples](docs/SAMPLES.md) — formal sample maturity and dependency boundaries
- [Compatibility Matrix](docs/COMPATIBILITY.md) — what has actually been validated
- [Troubleshooting](docs/TROUBLESHOOTING.md) — SDK / runtime / DPI problems and fixes
- [Benchmarks](docs/BENCHMARKS.md) — honest CPU and rendering-path measurements
- [Upgrading](docs/UPGRADING.md) — moving between SDK versions

Adapter integration guides:

- [Win32 Integration](docs/WIN32_INTEGRATION.md)
- [WPF Integration](docs/WPF_INTEGRATION.md)
- [WinUI 3 Integration](docs/WINUI3_INTEGRATION.md)

## Repository structure

```text
src/         Liquid-glass rendering/material engine, controls, motion
adapters/    Win32 / WPF / WinUI 3 integration
shaders/     HLSL shader sources
api/         Public API baseline manifests
cmake/       SDK install / package configuration
samples/     Runnable examples
tests/       Unit / integration / SDK validations
docs/        Consumer and engineering documentation
```

## License

Licensed under the [Apache License 2.0](LICENSE) (SPDX: `Apache-2.0`).

---

Maintainer workflow documentation: [`maintainer/`](maintainer/)
