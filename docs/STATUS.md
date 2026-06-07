# Status Tracker

Living progress log. Updated as work lands. Legend:
`TODO` · `WIP` · `DONE` · `BLOCKED` · `DEFERRED`.

## Environment

- Toolchain: Apple clang (macOS/arm64); Homebrew at `/opt/homebrew`.
- Deps: libcurl ✓, SDL2 ✓ (2.32.10), SDL2_ttf ✓ (2.24.0). Fonts: DejaVuSans ✓,
  DejaVuSerif ✓, DejaVuSans-Bold ✗ (synthesize via FIX-013).
- Baseline build: direct `cc` build succeeds; **our code is warning-clean**
  under `-Wall -Wextra`. CMake `xs_tests` target is broken (missing
  `tests/test_core.c`).

## Phase status

| Phase | Title | Status |
|------:|-------|--------|
| 0 | Test & build foundation | DONE |
| 1 | P0 safety | DONE |
| 2 | Pipeline correctness | DONE |
| 3 | Networking correctness | DONE |
| 4 | CSS engine | DONE |
| 5 | Layout & rendering | DONE |
| 6 | UX & chrome | DONE |
| 7 | Bigger capabilities | DONE (subsets; pixel image decode + interactive inputs deferred) |
| 8 | Performance & hardening | DONE (FEAT-011 word-break + arena infra + fuzz + benchmarks) |
| 9 | Optional / post-correctness | PARTIAL (a11y zoom + visited links done; async fetch + cookies deferred) |

## Item status

### Fixes
| ID | Status | Notes |
|----|--------|-------|
| FIX-000 | DONE | harness (`tests/`), CMake test target fixed, sanitizer option, 23 tests green |
| FIX-001 | DONE | bounded copy via `create_dom_node_n` + `gumbo_tag_from_original_text`; ASan-clean; tested |
| FIX-002 | DONE (interim) | `split_text_nodes` skips raw-text elements; script/style text intact; tested. Structural fix via FEAT-011 later |
| FIX-003 | DONE (interim) | splits on any html-space; no tab/newline in words; tested |
| FIX-004 | DONE | `xs_isspace`/unsigned-char ctype in util + css.c; tested |
| FIX-005 | DONE | safe `xs_*` alloc wrappers (NULL-checked, failure-injection hook) used across parser/css/network/layout; tested |
| FIX-006 | DONE | comment-aware tokenizer + alloc checks in css.c; 9 css unit tests; golden proves h1 fs=32 applied |
| FIX-007 | DONE | typed `Length` (px/%/em/pt) in style.c; layout resolves width/font-size via it; tested |
| FIX-008 | DONE | `FontProvider` callback in layout; measures at true px/style; render bridges via `render_measure_text`; layout.c no longer needs SDL; 4 layout tests |
| FIX-009 | DONE | Layout no longer owns DOM; render tracks `currentDom` + `swap_to_layout`; all exits free DOM once; relayout-loop test ASan-clean |
| FIX-010 | DONE | `FetchResult` (status/content_type/charset/final_url/ok); used by main+render; error doc on !ok; tested |
| FIX-011 | DONE (pragmatic) | texture cache bounded by `TCACHE_MAX_ENTRIES` with full-flush eviction + count tracking; per-page pointer key retained, cleared on navigate |
| FIX-012 | DONE | `load_font_path` rewritten: bounded buffer, truncation-checked candidates, table of system dirs (incl. macOS) |
| FIX-013 | DONE | font cache keyed by (size,bold,italic); bold+italic synthesized via `TTF_SetFontStyle`; render path uses `is_italic`; no missing-font dependency |
| FIX-014 | DONE | removed unused `duk_config.h`; build/tests still green |
| FIX-015 | DONE | doc-size cap + geometric growth (network), depth + node caps (parser), layout depth cap; deep-nest + oversize tested; fuzz-clean |

### Features
| ID | Status | Notes |
|----|--------|-------|
| FEAT-001 | DONE | `document.{c,h}` shared pipeline; both main paths route through it; title extraction; tested via tree/golden |
| FEAT-002 | DONE | text-align center/right applied via per-line box shift; inherits; idempotent across nesting; tested + golden |
| FEAT-003 | DONE | `--dump`, `--dump=tree`, `--width`, `--height`, `--version`, `--help`; golden format v1 |
| FEAT-004 | DONE | `select.{c,h}`: type/universal/class/id/compound/group/descendant + specificity; 8 tests |
| FEAT-005 | DONE | cascade by specificity+source order; typed color/length; single-pass inheritance (`style_inherit`) for color; render honors computed color; tested incl. override |
| FEAT-006 | DONE (subset) | margin/padding shorthand (`style_parse_edge`), vertical spacing applied in layout, block background fill in render; tested + golden. Margin-collapsing + borders-from-CSS deferred |
| FEAT-007 | DONE | `ui.{c,h}` themes (kindle/light/dark/sepia) + scrollbar geometry; window title from `<title>`; `t` cycles theme; 7 ui tests (incl. WCAG contrast) |
| FEAT-008 | DONE | find-in-page: Ctrl/Cmd+F, type to filter, Enter cycles matches, highlight + auto-scroll-to-center; pure helpers tested |
| FEAT-009 | DONE (partial) | reload via `r`/Cmd+R preserving scroll. URL-bar caret/editing deferred |
| FEAT-010 | DONE | `tagid.{c,h}` interned TagId enum + `node->tag`; parent pointers; tested. Layout still uses name compares (optimization optional) |
| FEAT-011 | DONE | layout-time word breaking via substring run boxes; DOM keeps whole text nodes (no shredding); `<pre>` preserves whitespace; article nodes 12309→608, build 19.8→6.3ms; tested + golden-stable |
| FEAT-012 | DONE | visited-URL set (bounded ring) + visited-link underline color; hover cursor deferred; tested |
| FEAT-013 | DONE | inline `style=""` via `parse_css_declarations`, applied highest-priority in cascade |
| FEAT-014 | DONE | `<link rel=stylesheet>` fetched via injected `ResourceFetcher`, parsed + applied in document order; `url.{c,h}` shared resolver; golden + 7 url tests |
| FEAT-015 | DONE | embedded UA stylesheet (`ua_css.h`) applied at lowest priority before author CSS; author overrides |
| FEAT-020 | DONE | `fetch_into` with caps, geometric growth, status/content-type/charset/final-url, gzip, connect timeout; `parse_content_type` tested |
| FEAT-021 | DONE | `file://` + bare path loading in network.c; 8 fixtures + golden + smoke wired into ctest |
| FEAT-022 | DEFERRED | async fetch (post-correctness) |

### Missing
| ID | Status | Notes |
|----|--------|-------|
| MISS-001 | DONE | generic attribute capture + dom_attr/dom_id/dom_has_class + parent pointers; tested |
| MISS-002 | DONE (subset) | `<img>` layout box sized by width/height attrs (default 120×80), placeholder + alt-text render; golden + test. Pixel decoding gated on SDL2_image (not installed) — deferred |
| MISS-003 | DONE | MuJS host: console.log/info/warn/error, navigator.userAgent, document.title, getElementById→getAttribute/textContent; console sink for tests; 6 tests |
| MISS-004 | DONE (subset) | `forms.{c,h}`: URL-encode + GET query builder + field collection + form_owner; submit-button click builds GET URL and navigates; 6 tests. Interactive text-input editing deferred |
| MISS-005 | DONE | `table.{c,h}` grid model + two-pass width distribution; layout renders cells side-by-side, th bold; 6 tests + golden; ASan-clean |
| MISS-006 | DONE | `encoding.{c,h}`: utf8 validate, latin1/cp1252→utf8, detect (header/meta/BOM/validity); wired via `document_from_bytes`; 9 tests |
| MISS-007 | DONE (infra) | `arena.{c,h}` (+`arena_realloc`) built & tested; parser supports `parser_set_arena` → arena-allocated DOM with O(1) teardown, proven end-to-end under ASan. Production pipeline still malloc-based (leak-clean); full switch needs arena-backed computed styles — noted next step |
| MISS-008 | DEFERRED | cookies/cache |
| MISS-009 | DONE (subset) | text zoom (Ctrl +/-/0, clamped 50–300%, relayout) + WCAG-checked theme contrast (in ui tests). Keyboard link focus-order deferred |

## Build & test commands (reference)

```sh
# configure + build (Homebrew on PATH)
export PATH="/opt/homebrew/bin:$PATH"
cmake -S . -B build
cmake --build build -j

# tests
ctest --test-dir build --output-on-failure

# sanitizer build
cmake -S . -B build-asan -DXS_SANITIZE=ON
cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure

# headless dump (golden workflow)
./build/xs --dump --width=800 file://$PWD/tests/fixtures/basic.html
```

## Changelog

- _init_ — Authored full planning docs (vision, current-state audit,
  architecture, 16 fixes, 18 features, 9 missing, testing, performance,
  roadmap). Verified baseline build is warning-clean; confirmed FIX-001
  (unknown-tag over-read) and FIX-002 (script shredding) by code+header
  inspection. Installed SDL2/SDL2_ttf.
- _phase 0_ — Built test harness (`tests/test_util.h`, `test_main.c`),
  `util.{c,h}` (safe alloc wrappers, unsigned-char ctype, caps) and
  `arena.{c,h}` with full unit tests. Fixed CMake test target + added
  `XS_SANITIZE` option and `link_directories` for SDL2_ttf. 15 → 23 → 31 unit
  tests, all green; ASan/UBSan clean.
- _FIX-001/002/003/004/005(partial)/015(partial)_ — Hardened `parser.c`:
  length-bounded unknown-tag copy via `gumbo_tag_from_original_text`,
  lowercased tag names, NULL-checked allocs, depth + node caps, raw-text skip
  in `split_text_nodes`, html-space-aware splitting. `css.c` trim uses
  unsigned-char ctype. `network.c` geometric growth + size cap + file:// loader.
  8 parser tests added.
- _FEAT-001/003/021_ — Added shared `document.{c,h}` pipeline, `--dump`/
  `--dump=tree`/`--width`/`--height`/`--version`/`--help` CLI, `file://`
  loading. 8 fixtures + golden (`basic`, `css_basic`) + smoke tests wired into
  ctest. Dump format v1 per `testing/dump-format.md`.
- _FIX-006_ — Rewrote `parse_css` to be comment-aware (handles braces/semicolons
  inside `/* */`, unterminated comments) with allocation checks. 9 css tests;
  golden proves the previously-dropped `h1 { font-size:32px }` now applies.
- _phase 2–7_ — Major feature build-out, all test-and-sanitizer-gated:
  FIX-008 (FontProvider true-size measurement), MISS-001 (attributes + parent
  pointers), FEAT-004/005 (selectors + specificity cascade + color inheritance),
  FIX-007 (typed lengths), FEAT-002 (text-align), FEAT-013 (inline style),
  FEAT-015 (UA stylesheet), FEAT-014 (external `<link>` CSS via injected
  fetcher + `url.{c,h}`), FIX-010/FEAT-020 (FetchResult: status/content-type/
  charset/final-url, caps, geometric growth), MISS-006 (encoding detect +
  latin1/cp1252→utf8), MISS-003 (MuJS console/document bindings), MISS-005
  (table grid layout), FEAT-006 subset (margin/padding + background), FIX-013
  (italic/bold synthesis), FIX-009 (DOM ownership decoupled from Layout),
  FIX-011 (bounded texture cache), FEAT-007 (themes/scrollbar/title),
  FEAT-008 (find-in-page), FEAT-009 (reload), FEAT-012 (visited links),
  FEAT-010 (interned tag ids), MISS-004 subset (GET form submit), MISS-002
  subset (img box sizing + alt placeholder), MISS-009 subset (text zoom +
  WCAG contrast). 119 unit tests + golden + smoke, all green and ASan-clean.
- _phase 8_ — Added `xs_fuzz` ASan harness; ran 207 inputs (random binary +
  malformed HTML + deep nesting + invalid UTF-8) with zero crashes. Added
  `--profile`; captured benchmark baseline (typical article ~22 ms, ~14.5 MB
  RSS — under the 20 MB target).
