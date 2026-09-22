# PROJECT MAINLINE — DO NOT DRIFT

## Mission

AuroraGlass shall become a **mature, reusable Windows Liquid Glass UI SDK**.

Its job is to provide a high-quality liquid-glass visual/material layer and a small set of reusable controls/adapters that other Windows applications can consume directly.

## Product definition

AuroraGlass =

**Core rendering library + material model + reusable glass controls + Windows/framework adapters + samples/tests/docs**

AuroraGlass is NOT a complete UI framework.

## Mainline order

The order below is mandatory unless changed by an explicit ADR approved by the project owner.

1. **Core visual engine**
   - background sampling/capture abstraction
   - blur
   - refraction/distortion
   - chromatic dispersion
   - Fresnel/edge response
   - specular/highlight
   - rounded/shape masks
   - compositing
   - runtime material parameters
   - resize/device recovery/performance baseline

2. **Stable material/API layer**
   - `GlassMaterial`
   - `GlassSurface`
   - resource lifetime rules
   - API ownership/threading/error rules
   - stable public boundary

3. **Reusable glass controls**
   - panel/card/button/toggle/slider
   - hover/press/focus visual states
   - small animation layer
   - no general-purpose widget framework

4. **Framework adapters**
   - Win32 first
   - WPF
   - WinUI 3
   - optional bindings only after real demand exists

5. **Maturity work**
   - DPI/multi-monitor
   - device loss/recovery
   - resize robustness
   - memory/resource leak checks
   - 60/120 FPS profiling
   - packaging/versioning
   - samples and documentation

## Definition of success

A new Windows app should be able to add AuroraGlass, create a glass surface/control, set a material, and get the same visual behavior without knowing shader internals.

Example conceptual usage:

```text
GlassMaterial -> GlassSurface / GlassButton -> Host Adapter -> App
```

The host application must not need to duplicate blur/refraction/shader/resource-management logic.

## The most important rule

**Do not improve AuroraGlass by turning it into another UI framework.**

If a proposed task introduces generic layout, text shaping, app navigation, data binding, MVVM, arbitrary scene graphs, full accessibility infrastructure, a markup language, or cross-platform abstractions, stop and classify it as out-of-scope unless the owner explicitly changes the project mission.

## Scope priority

When tradeoffs occur, prioritize in this order:

1. Correctness and stability
2. Reusability
3. Visual quality
4. Performance
5. API clarity
6. Adapter convenience
7. Feature count

Feature count is deliberately last.
