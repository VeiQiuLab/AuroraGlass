# Global Acceptance Criteria

These criteria apply whenever relevant to the current phase.

## Correctness
- no undefined behavior or known use-after-free in normal lifecycle
- clear error propagation
- parameter validation
- clean create/destroy lifecycle

## Rendering
- no persistent stale frames during resize/move
- no obvious mask seams
- no uninitialized shader/resource reads
- effect can be disabled/fallback safely

## Windows behavior
- resize works
- minimize/restore works
- DPI changes do not corrupt output
- multi-monitor move does not crash
- device recreation path is defined

## Performance
- benchmark methodology documented
- CPU/GPU timing separated where practical
- no unbounded allocation per frame
- no known per-frame resource leaks
- target 60 FPS baseline for supported sample configuration; 120 FPS measured where hardware permits

## API quality
- host app does not need shader knowledge
- Core does not depend on WPF/WinUI
- adapters do not duplicate the render engine
- public names and ownership rules documented

## Release quality
Before declaring SDK maturity:
- clean build from documented prerequisites
- minimal sample for every supported adapter
- versioned package/artifact
- troubleshooting documentation
- compatibility matrix
