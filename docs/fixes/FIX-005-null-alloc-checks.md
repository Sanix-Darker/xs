# FIX-005 — Unchecked malloc/calloc/realloc

- **Severity:** P0 (NULL deref / crash under memory pressure)
- **Files:** `parser.c`, `css.c`, `render.c`, `layout.c`

## Problem

Many allocations are dereferenced without a NULL check:

- `parser.c create_dom_node`: `calloc(...)` then immediately `node->name = ...`
  with no check.
- `parser.c split_text_nodes`: `malloc(len+1)` for each word, `new_children`
  realloc — unchecked.
- `css.c`: `malloc(sizeof(CSSStyleSheet))`, `trim`'s `malloc`, the `realloc` of
  `declarations` and `rules` — all unchecked (and `realloc` into the same
  pointer leaks on failure).
- `render.c tcache_insert`: `malloc(sizeof *e)` unchecked.

Under low memory or a hostile huge page (FIX-015), any of these returns NULL and
the next write crashes.

## Evidence

Grep for `alloc(` across the four files; none are guarded.

## Micro-plan

1. Introduce safe wrappers in a shared `util.h`:
   - `xmalloc/xcalloc/xrealloc` that, on failure, return NULL (never abort) and
     the **callers** handle NULL by failing the operation gracefully.
   - For the realloc-into-same-pointer leak, use a `grow()` helper that takes
     `void** ptr` and only assigns on success.
2. Once the arena (PERF-002) lands, most DOM/string allocs route through the
   arena, whose single failure point is checked once per block.
3. Every caller must have a defined behavior on allocation failure: return
   NULL/false up the stack so the pipeline shows the error document instead of
   crashing.

## Acceptance tests

- Unit test with an injected failing allocator (compile-time hook
  `XS_TEST_ALLOC_FAIL`) verifies `parse_css`, `create_dom_node`, and layout
  return cleanly (NULL/empty) instead of crashing when allocation fails.
- ASan/LeakSan: the realloc-failure path does not leak the original buffer.

## Risk

Low. Defensive. The injected-failure test is the main effort.
