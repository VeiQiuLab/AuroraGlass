# Scope Guardrails

## In scope for v1

- Windows 10/11 desktop applications
- GPU accelerated liquid-glass material
- Blur and translucent compositing
- Refraction/distortion
- Chromatic dispersion / subtle RGB separation
- Fresnel-style edge response
- Specular/highlight layer
- Tint/saturation/brightness/noise controls where useful
- Shape/radius masks
- Runtime parameter updates
- Reusable `GlassMaterial` and `GlassSurface`
- Small glass control set
- Hover/press/focus motion needed by those controls
- Win32 host support
- WPF and WinUI 3 adapters after Core stabilization
- DPI scaling, resizing, multi-monitor behavior
- device-lost recovery
- profiling and leak testing
- samples, docs, packaging

## Explicitly out of scope for v1

Do NOT add these merely because they look useful:

- Cross-platform support
- Linux/macOS/iOS/Android/Web backends
- Custom UI markup language
- Custom XAML dialect
- General layout engine
- General text engine / text shaping stack
- MVVM framework
- State-management framework
- Dependency injection framework
- App navigation framework
- General-purpose scene graph
- Full window manager
- Plugin marketplace
- Theme marketplace
- Web renderer
- Browser engine
- Game engine
- Live2D/runtime features
- AI integration
- Network services
- updater/telemetry/account system

## Scope test

Before implementing any new subsystem, answer:

> Does this capability directly make liquid-glass rendering reusable, stable, visually correct, performant, or easy to host in a Windows application?

If the answer is not clearly yes, do not implement it.

## Dependency rule

Prefer using the host framework for generic UI responsibilities.

- WPF handles WPF layout/text/input patterns.
- WinUI handles WinUI layout/text/input patterns.
- Win32 owns native window/event plumbing.
- AuroraGlass provides glass rendering/material/control integration.

## New idea quarantine

Any attractive but non-mainline idea must be written under `status/BACKLOG.md` first. It must not be implemented during the current phase.
