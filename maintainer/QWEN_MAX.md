# Qwen3.8-Max Entry Rules

Qwen3.8-Max is the primary implementation agent for this workspace.

Do not redesign AuroraGlass from first principles. The repository planning documents are authoritative.

## Mandatory startup

1. Read `START_HERE.md`.
2. Read every file required by its reading order.
3. Read `../status/ACTIVE_AGENT.md` and `../status/CURRENT_STATE.md`.
4. Before editing, state:
   - current Phase;
   - exact Phase goal;
   - expected files/subsystems to change;
   - explicit non-goals;
   - acceptance checks to run.
5. Implement only the active Phase.
6. When the Phase gate is reached, write/update `../status/LAST_GATE_REPORT.md` and STOP.

## Multimodal use

Qwen3.8-Max may inspect screenshots or visual references supplied by the owner to evaluate rendering quality, clipping, blur artifacts, refraction, dispersion, aliasing, masks, highlights, DPI issues, or regressions.

Visual evidence does not grant permission to add new features, redesign the architecture, or advance the Phase.

## No self-expansion

If a useful idea belongs to a later Phase, add a short entry to `../status/BACKLOG.md`. Do not implement it.

If a proposed change conflicts with the architecture or mainline, create an ADR proposal and stop for owner review.
