# AuroraGlass

AuroraGlass is a Windows-only Liquid Glass UI SDK.

The project exists to provide a reusable, high-quality liquid-glass rendering/material system that can be embedded into multiple Windows applications without requiring each application to reimplement the effect.

## One-line mainline

**Build a reusable Windows Liquid Glass SDK: Core Library -> Controls -> Framework Adapters -> Samples/Docs. Do not become a full UI framework.**

## Read this first

Any AI agent or contributor must read these files before making changes:

1. `PROJECT_MAINLINE.md`
2. `SCOPE_GUARDRAILS.md`
3. `ROADMAP.md`
4. `PHASE_GATE.md`
5. `status/CURRENT_STATE.md`
6. `AGENTS.md`

If these documents conflict with an implementation idea, the documents win unless an explicit ADR changes the decision.

## Target structure

```text
AuroraGlass
├─ src/core              # Liquid-glass rendering/material engine
├─ src/controls          # Reusable glass controls
├─ src/animation         # Interaction/motion layer
├─ src/platform/win32    # Windows platform integration
├─ shaders               # HLSL shader code
├─ adapters/win32        # Win32 convenience adapter
├─ adapters/wpf          # WPF adapter
├─ adapters/winui3       # WinUI 3 adapter
├─ samples               # Minimal runnable examples
├─ tests                 # Unit/integration/perf tests
├─ docs                  # Engineering documentation
├─ decisions             # ADRs
├─ prompts               # AI execution templates
└─ status                # Current phase and project ledger
```

## Scope baseline

- Windows 10/11 x64 first.
- Real-time liquid-glass rendering.
- GPU-accelerated implementation.
- Reusable SDK/API surface.
- First-class Win32 integration.
- WPF and WinUI 3 adapters after Core is stable.
- No cross-platform requirement in v1.
- No custom XAML, layout engine, text engine, MVVM framework, window manager, or general-purpose UI runtime.

## Current phase

See `status/CURRENT_STATE.md`.
