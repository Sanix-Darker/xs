# FIX-015 — No bounds on size, depth, or node count

- **Severity:** P0 (OOM / stack overflow on hostile input)
- **Files:** `network.c`, `parser.c`, `layout.c`

## Problem

Nothing limits resource use from network input:

1. **Response size:** `network.c` appends every byte into a growing buffer with
   no cap. A multi-GB or chunked-forever response exhausts memory.
2. **Node count:** `parse_html` + `split_text_nodes` build one node per element
   and (currently) one per word. A document of millions of tokens creates
   millions of nodes → OOM. `split_text_nodes` makes this dramatically worse.
3. **Recursion depth:** `parse_gumbo_node`, `layout_node`, and `free_dom` are
   recursive with no depth cap. Deeply nested HTML (`<div><div>...` thousands
   deep — a known fuzzing/DoS vector) overflows the C stack and crashes.
4. **Buffer growth:** `network.c` reallocs by exactly the new chunk size, which
   is O(n^2) memcpy on large bodies (perf, not safety, but related).

## Evidence

`network.c write_callback` (linear realloc, no cap); recursive walkers in
`parser.c`/`layout.c` with no depth parameter.

## Micro-plan

1. **Size cap:** add `XS_MAX_DOCUMENT_BYTES` (default e.g. 32 MB, configurable).
   `write_callback` aborts the transfer (`return 0` after cap) and the page
   shows "document too large". Use geometric buffer growth (×1.5/×2) to fix the
   O(n^2) copy.
2. **Depth cap:** thread a `depth` int through `parse_gumbo_node` and
   `layout_node`; beyond `XS_MAX_DEPTH` (e.g. 512) stop descending (treat
   deeper content as flattened/ignored). Convert `free_dom` to an explicit
   stack/iterative free (or rely on the arena, which makes recursive free moot —
   PERF-002).
3. **Node cap:** add `XS_MAX_NODES` (e.g. 2,000,000) — when exceeded, stop
   adding nodes (page is truncated but the browser stays alive).
4. Removing per-word shredding (FEAT-011) cuts node counts by ~5–10× on text.

## Acceptance tests

- Unit: parsing a synthetic string of 5,000 nested `<div>`s does not crash and
  respects the depth cap (resulting tree depth ≤ cap).
- Unit: a synthetic 40 MB body is rejected by the size cap path (simulate by
  feeding the cap a small value via a test hook).
- ASan/stack: deep-nesting test runs without stack overflow.

## Risk

Medium. Depth capping changes output for pathological pages (acceptable — they
were unrenderable anyway). The arena conversion is the bigger lever and is
tracked separately.
