# AGENTS.md — AuroraGlass AI Rules

These instructions apply to the active implementation agent.

For this workspace, the primary and only implementation model is **Qwen3.8-Max** unless the project owner explicitly changes `status/ACTIVE_AGENT.md`.

## Required reading order

Before editing code, read:

1. `PROJECT_MAINLINE.md`
2. `SCOPE_GUARDRAILS.md`
3. `ROADMAP.md`
4. `PHASE_GATE.md`
5. `status/CURRENT_STATE.md`
6. relevant ADRs in `decisions/`

## Primary objective

Build a reusable Windows Liquid Glass UI SDK.

Do not reinterpret the project as a general-purpose UI framework.

## Current-phase lock

Only implement requirements belonging to the phase identified in `status/CURRENT_STATE.md` unless the user explicitly instructs otherwise.

Later-phase ideas go to `status/BACKLOG.md`. Do not implement them early.

## No silent scope expansion

Never add a subsystem merely because it may be useful later.

Examples of prohibited unsolicited expansion:
- cross-platform abstraction
- custom layout engine
- custom text stack
- custom markup/XAML
- navigation framework
- MVVM framework
- plugin system
- updater
- telemetry
- AI features

## Architecture discipline

- Core rendering/material code must remain host-framework agnostic.
- Adapters depend on Core; Core must not depend on WPF/WinUI.
- Controls reuse the same Core rendering path.
- Avoid duplicated shader/effect implementations across adapters.
- Prefer explicit ownership and lifetimes.
- Any public API break must be stated in the completion report.

## Change discipline

Avoid unrelated cleanup while implementing a phase. If cleanup is desirable but not required, record it in `status/BACKLOG.md`.

For a major architecture change:
1. Stop implementation.
2. Create an ADR proposal.
3. Explain current problem, options, tradeoffs, migration cost.
4. Wait for owner approval before proceeding.

## Truthfulness

Never claim a test, benchmark, build, or runtime path passed unless it was actually executed and its result was observed.

Never substitute synthetic success for real integration evidence when the phase requires real behavior.

## Completion behavior

When the assigned phase or task is complete, write a concise gate report and STOP. Do not continue into the next phase without explicit instruction.
