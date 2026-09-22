# Architecture

## Dependency direction

```text
App / Host Framework
        |
        v
Framework Adapter (Win32 / WPF / WinUI3)
        |
        v
Controls / Animation
        |
        v
AuroraGlass Core
        |
        v
Windows Graphics Backend + HLSL
```

Dependencies point downward only.

## Core responsibilities

Core owns:
- graphics device/context abstraction
- shader/effect pipeline
- glass material representation
- glass surface rendering
- GPU resources
- effect parameter validation
- compositing
- lifecycle/recovery primitives
- render statistics needed for diagnostics

Core does not own:
- application navigation
- generic layout
- generic text editing
- MVVM/data binding
- business logic
- application state architecture

## Material model

The public material model should remain compact and stable. Candidate properties:

```text
blur_radius
refraction_strength
dispersion_strength
thickness
edge_fresnel
specular_strength
highlight_position / light parameters
tint
saturation
brightness
noise_amount
corner_radius / shape mask parameters
opacity
```

Parameters should be validated and bounded. Internal shader constants may differ from public API representation.

## Render pipeline concept

```text
Background Source
  -> Preprocess / downsample if needed
  -> Blur
  -> Refraction sample offset
  -> Dispersion
  -> Fresnel / edge response
  -> Specular / highlight
  -> Tint / saturation / brightness
  -> Shape mask
  -> Composite
```

The exact implementation may evolve, but the public API must not expose internal pass structure unless necessary.

## Host integration

Adapters are thin integration layers. Their job is lifecycle, surfaces, DPI, resize, and host input/event bridging.

They must not fork the visual implementation.

## Language/ABI policy

The project mainline does not require one implementation language forever. Choose the smallest practical Windows-native stack for the active phase.

However:
- HLSL is expected for GPU effects.
- Public cross-language boundaries should remain explicit and stable.
- A change of core language or ABI requires an ADR once the Core public API exists.

## Performance principle

Do not blur/capture the same background independently for every control when a shared intermediate can safely be reused. Optimize based on profiling, not guesses.
