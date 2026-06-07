# Roadmap

Phased plan. Ordering obeys two rules: **correctness/stability before
capability**, and **foundations before the things that depend on them**. The
browser must build and pass tests at the end of every phase.

Dependency-critical foundations (test harness, arena, attributes, shared
pipeline, `--dump`/fixtures) come first because almost everything else leans on
them.

## Phase 0 — Test & build foundation (unblocks everything)

Goal: a green, headless, sanitizer-capable test loop.

- FIX-000 — test harness + `tests/` + fix CMake test target
- FEAT-003 — CLI flags (`--dump`, `--width`, `--version`, `--help`)
- FEAT-021 — `file://`/local loading + first fixtures
- testing/dump-format contract honored by `--dump`

Exit criteria: `ctest` runs headlessly and is green; `--dump` produces stable
output for `basic.html`; sanitizer build option works.

## Phase 1 — P0 safety (stop the crashes/corruption)

Goal: no memory corruption, no OOM on hostile input.

- FIX-001 — unknown-tag heap over-read
- FIX-005 — alloc NULL checks
- FIX-004 — isspace UB
- FIX-015 — input bounds (size/depth/node caps) + geometric net growth
- MISS-007 — arena allocator (also simplifies frees / FIX-015 recursion)
- FIX-009 — finalize DOM ownership (Document owns arena; Layout borrows)

Exit criteria: ASan/UBSan/LeakSan clean on the corpus; deep-nest and
huge-input fixtures don't crash; navigation/relayout loop leak-free.

## Phase 2 — Pipeline correctness

Goal: faithful text and a single, correct load path.

- FEAT-001 — shared `build_document` (kills the duplicate pipeline)
- FEAT-011 — layout-time word breaking (removes shredding) → fixes FIX-002,
  FIX-003 structurally; big memory win
- FIX-002 / FIX-003 — verified fixed by the above (tests)
- FIX-008 — measure text at true font size (font provider)
- FEAT-010 — interned tags + parent pointers
- MISS-001 — generic attribute capture
- MISS-006 — encoding detection + transcode

Exit criteria: scripts/CSS text intact; whitespace correct; headings wrap
correctly; non-UTF-8 fixtures legible; node counts O(structure).

## Phase 3 — Networking correctness

- FEAT-020 / FIX-010 — FetchResult (status/content-type/charset/final-url),
  caps, geometric growth, handle reuse
- error/`non-HTML` handling via the error document

Exit criteria: 404/binary/redirect handled sanely; relative URLs resolve
against the final URL; live smoke (guarded) works.

## Phase 4 — CSS engine

- FIX-006 — comment-aware tokenizer
- FIX-007 — typed lengths (units/%)
- FEAT-005 — typed cascade (color/specificity/inheritance/origin)
- FEAT-004 — class/id/group/descendant/universal selectors
- FEAT-013 — inline `style=""`
- FEAT-015 — UA stylesheet (replaces magic constants)
- FEAT-014 — external `<link>` CSS

Exit criteria: selector/cascade/color/length unit tests green; styled fixtures
match goldens; magic spacing constants replaced by UA CSS at parity.

## Phase 5 — Layout & rendering correctness

- FEAT-002 — text-align applied
- FEAT-006 — box model (margin/padding/border, collapsing)
- FIX-013 — italic synthesis + bold (no missing-font dependency)
- FIX-011 — content-keyed, bounded texture cache
- FIX-012 — font path robustness

Exit criteria: alignment/box-model goldens green; emphasis visually distinct;
texture cache bounded and correct across navigation.

## Phase 6 — UX & chrome

- FEAT-007 — scrollbar, title, status, themes
- FEAT-009 — reload + URL-bar caret/editing
- FEAT-012 — visited links + hover cursor
- FEAT-008 — find-in-page

Exit criteria: chrome/logic unit tests green; manual UX pass; smoke clean.

## Phase 7 — Bigger capabilities

- MISS-005 — tables (grid first)
- MISS-002 — images (optional dep, capped)
- MISS-003 — JS host bindings (console + read-only document)
- MISS-004 — forms (GET + text/button)

Exit criteria: each ships with unit + golden + smoke; caps enforced; no
regressions.

## Phase 8 — Performance & hardening pass

- PERF-003 — compact structs (bitfield hints, shrink node)
- PERF-004 — measurement cache
- PERF-006 — incremental relayout on resize
- testing/fuzzing — fuzz harness + soak; fold crashes into regression fixtures
- benchmarks — fill baseline + post-change numbers; verify vision targets

Exit criteria: vision metrics met (RSS/start/relayout); fuzz soak clean;
benchmark log populated.

## Phase 9 — Optional / post-correctness

- FEAT-022 — async fetch thread
- MISS-008 — cookies + in-memory cache
- MISS-009 — accessibility (zoom/contrast/focus order)
- FIX-014 — remove dead `duk_config.h` (can be done anytime; trivial)
- docs — update top-level `README.md` to match reality

## Cross-cutting (every phase)

- Update `STATUS.md` as items move.
- Keep the build warning-clean (our code) and `ctest` green.
- Record perf numbers when touching perf-relevant code.
