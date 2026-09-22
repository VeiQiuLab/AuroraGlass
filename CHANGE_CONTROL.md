# Change Control

## Three classes of change

### Class A — implementation detail
No ADR required if it stays within current phase and does not alter public architecture.

### Class B — public API change
Document reason, compatibility impact, and migration notes in the phase report. ADR recommended after API stabilization begins.

### Class C — mainline/architecture change
ADR required before implementation.

Examples:
- becoming cross-platform
- replacing the rendering backend
- adding a general UI framework layer
- changing core language/ABI after stabilization
- changing dependency direction
- adding a major third-party runtime dependency

## ADR format

Every ADR should include:
- Context
- Decision
- Alternatives considered
- Consequences
- Migration impact
- Status: Proposed / Accepted / Rejected / Superseded
