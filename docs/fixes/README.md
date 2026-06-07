# Fixes — Correctness & Stability Defects

Each `FIX-xxx` is a defect in existing behavior: a crash, a memory error, wrong
output, or a build problem. Features that add new capability live in
[`../features/`](../features/README.md); absent subsystems live in
[`../missing/`](../missing/README.md).

## Index

| ID | Severity | Title | File(s) |
|----|----------|-------|---------|
| [FIX-000](FIX-000-test-harness.md) | P1 | Broken test target / no tests exist | `CMakeLists.txt` |
| [FIX-001](FIX-001-gumbo-unknown-tag.md) | P0 | Heap over-read on unknown tag names | `parser.c` |
| [FIX-002](FIX-002-script-text-shredded.md) | P1 | JS source destroyed by word-splitting | `main.c`, `render.c` |
| [FIX-003](FIX-003-whitespace-handling.md) | P1 | Tabs/newlines mishandled in text split | `parser.c` |
| [FIX-004](FIX-004-isspace-ub.md) | P1 | `isspace(char)` UB on signed chars | `css.c`, `layout.c` |
| [FIX-005](FIX-005-null-alloc-checks.md) | P0 | Unchecked malloc/calloc/realloc | many |
| [FIX-006](FIX-006-css-comments.md) | P1 | CSS comments desync the parser | `css.c` |
| [FIX-007](FIX-007-css-units.md) | P1 | CSS lengths ignore units and `%` | `layout.c`, `css.c` |
| [FIX-008](FIX-008-text-measure.md) | P1 | Text measured at wrong font size | `layout.c` |
| [FIX-009](FIX-009-dom-ownership.md) | P1 | Fragile DOM ownership / free paths | `render.c`, `layout.c` |
| [FIX-010](FIX-010-http-status.md) | P1 | HTTP status/content-type not surfaced | `network.c` |
| [FIX-011](FIX-011-texture-cache-key.md) | P1 | Texture cache keyed by raw pointer | `render.c` |
| [FIX-012](FIX-012-path-truncation.md) | P3 | Font path buffer truncation | `render.c` |
| [FIX-013](FIX-013-italic-bold-fonts.md) | P1 | Italic never synthesized; bold font missing | `render.c` |
| [FIX-014](FIX-014-dead-duk-config.md) | P3 | Unused `duk_config.h` dead file | repo root |
| [FIX-015](FIX-015-input-bounds.md) | P0 | No bounds on size/depth/node count | `network.c`, `parser.c` |

## Working rule

A fix is complete only when it has: a reproduction, a code change, a regression
test in `tests/`, and a clean ASan run where relevant.
