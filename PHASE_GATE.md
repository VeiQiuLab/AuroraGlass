# Phase Gate Protocol

This document is mandatory for AI-driven development.

## Before coding

The agent must state:

1. Current phase
2. Exact phase goal
3. Files/subsystems expected to change
4. Explicit non-goals for this phase
5. Acceptance tests it intends to satisfy

If the requested task does not belong to the current phase, the agent must stop and report the mismatch instead of silently expanding scope.

## During coding

The agent must not:

- start work assigned to a later phase
- perform broad refactors unrelated to the active acceptance criteria
- replace working architecture solely for elegance
- introduce a framework or dependency without a concrete current-phase need
- change public API and architecture simultaneously without documenting why
- optimize speculative bottlenecks before profiling

## Phase completion report

At the end of a phase, report exactly:

- Completed requirements
- Acceptance tests run
- Known limitations
- Files/modules added or changed
- Architecture changes, if any
- Deferred items
- Whether phase gate PASS or FAIL

Then STOP.

Do not automatically start the next phase.

## Gate states

- `NOT_STARTED`
- `ACTIVE`
- `BLOCKED`
- `REVIEW`
- `PASS`
- `FAIL`

Only the project owner advances the phase from PASS to the next phase.

## Failure behavior

When a gate fails, fix only failures needed for that gate. Do not compensate by rewriting unrelated systems.
