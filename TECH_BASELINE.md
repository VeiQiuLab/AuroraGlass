# Technical Baseline

This baseline exists to prevent agents from repeatedly changing the stack before useful work is completed.

## v1 platform

- Windows 10/11 desktop
- x64 first

## P0/P1 rendering baseline

- Native implementation: C++20
- Graphics API: Direct3D 11
- Shaders: HLSL
- DXGI where required for surfaces/background sources
- DirectComposition may be used where it materially improves native composition, but it is not a reason to redesign the project

## Why this baseline

- direct access to the Windows graphics stack
- simple interop boundary for Win32/WPF/WinUI adapters
- avoids coupling Core to one managed UI framework
- HLSL/D3D11 are sufficient to validate the material before considering backend expansion

## Interop direction

After Core behavior is proven:

```text
C++ Core
  -> explicit native public boundary
  -> Win32 adapter
  -> WPF / WinUI 3 adapter
```

Managed adapters may use C#/interop as appropriate, but must not duplicate the rendering engine.

## Not allowed without ADR

Before v1 Core stabilization, do not replace the baseline with:
- Vulkan
- OpenGL
- WebGPU
- a cross-platform renderer
- a game engine
- an embedded browser renderer
- a full Rust rewrite
- a full C# renderer rewrite

A future Rust convenience binding is allowed as backlog work, not as a prerequisite for v1.
