# AuroraGlass Roadmap

Each phase must pass its gate before the next begins. An agent must stop at the end of the assigned phase.

## P0 — Visual Core Proof

Goal: one robust, real-time liquid-glass surface.

Required:
- Windows native rendering path initialized successfully
- background input abstraction
- blur
- refraction/distortion
- chromatic dispersion
- Fresnel/edge light
- specular/highlight
- rounded-rectangle mask
- runtime material parameter changes
- resize support
- basic FPS/resource telemetry

Exit condition:
- sample can run continuously without obvious corruption/leaks
- visual stages can be toggled independently
- no controls/framework work yet

## P1 — Core Library

Goal: turn the proof into reusable engine code.

Required:
- `GlassMaterial`
- `GlassSurface`
- renderer/device/resource modules
- documented ownership and threading model
- error/result model
- deterministic resource cleanup
- device-lost/recreate path
- tests around material validation and resource lifetime

Exit condition:
- sample app consumes the public Core API rather than shader internals
- no host-specific UI control assumptions inside Core

## P2 — Core Stabilization

Goal: make Core dependable before building UI wrappers.

Required:
- DPI handling
- multi-monitor validation
- resize/minimize/restore validation
- performance baselines at 1080p and 1440p
- repeated create/destroy stress test
- GPU/CPU resource leak checks
- parameter boundary tests
- API review/freeze candidate

Exit condition:
- public Core API marked `v0.1-stable-candidate`

## P3 — Glass Controls

Goal: reusable glass controls without inventing a UI framework.

Initial controls:
- `GlassPanel`
- `GlassCard`
- `GlassButton`
- `GlassToggle`
- `GlassSlider`

Required states:
- normal
- hover
- pressed
- disabled
- focus where the host needs it

Exit condition:
- controls reuse Core; no duplicate shader pipelines
- generic layout/text responsibilities remain in host framework

## P4 — Motion Layer

Goal: controlled visual motion.

Required:
- small spring/tween primitives
- hover/press transitions
- highlight/light-follow behavior
- interruption-safe animation
- reduced-motion capability

Exit condition:
- animation module remains optional and small

## P5 — Win32 Adapter

Goal: first complete host integration.

Required:
- simple window attachment lifecycle
- size/DPI/event plumbing
- input bridge needed by glass controls
- sample application
- clear teardown behavior

Exit condition:
- fresh Win32 app can integrate AuroraGlass from documented steps

## P6 — WPF Adapter

Goal: WPF consumption without rewriting Core.

Required:
- interop boundary
- host lifecycle
- DPI/resize/input behavior
- sample
- documentation

Exit condition:
- WPF sample uses the same Core material model

## P7 — WinUI 3 Adapter

Same principles as P6, adapted to WinUI 3.

## P8 — SDK Maturity

Goal: distributable SDK.

Required:
- versioned public API
- package/build instructions
- compatibility matrix
- samples
- troubleshooting docs
- benchmark docs
- upgrade notes
- semantic versioning rules

Exit condition:
- another project can consume AuroraGlass without reading internal source.

## P9+ — Optional backlog only

Potential work only after v1 quality is reached:
- additional shapes
- additional host adapters
- Rust convenience bindings
- additional material presets
- advanced lighting models

None of these may block v1.
