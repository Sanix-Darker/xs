# xs Browser — Engineering Documentation

This directory is the single source of truth for the `xs` browser overhaul. It
documents the project vision, the current state of the code, every known bug,
every planned feature, everything that is missing, the testing strategy, the
performance strategy, and the implementation roadmap.

The mission: build the **strongest, most stable, lightest, fastest, lowest
memory-footprint** graphical web browser we can in portable C.

## How to read these docs

Read in this order:

| # | Document | Purpose |
|---|----------|---------|
| 1 | [`00-VISION.md`](00-VISION.md) | What we are building and the non-negotiable principles |
| 2 | [`01-CURRENT-STATE.md`](01-CURRENT-STATE.md) | Honest audit of the code as it exists today |
| 3 | [`02-ARCHITECTURE.md`](02-ARCHITECTURE.md) | Current pipeline + target architecture |
| 4 | [`ROADMAP.md`](ROADMAP.md) | Phased implementation plan with ordering |
| 5 | [`STATUS.md`](STATUS.md) | Living progress tracker (updated as work lands) |

Then the detailed work breakdowns:

- [`fixes/`](fixes/README.md) — correctness bugs and stability defects (FIX-xxx)
- [`features/`](features/README.md) — features to add or complete (FEAT-xxx)
- [`missing/`](missing/README.md) — capabilities absent entirely (MISS-xxx)
- [`testing/`](testing/README.md) — test plan, matrix, and harness design
- [`performance/`](performance/README.md) — memory and speed strategy

## Conventions

- Every work item has a stable ID (`FIX-001`, `FEAT-003`, `MISS-002`, ...).
- Every item lists: problem, evidence, micro-plan, acceptance tests, risk.
- IDs are referenced from `ROADMAP.md` and `STATUS.md`.
- Severity: `P0` (crash/corruption), `P1` (wrong output), `P2` (missing
  capability), `P3` (polish).

## Quality bar

Nothing is "done" until:
1. It builds clean with `-Wall -Wextra` (no new warnings in our code).
2. It has at least one automated test in `tests/`.
3. The full test suite passes (`ctest`).
4. It runs clean under AddressSanitizer where applicable.
