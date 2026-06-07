# Performance & Memory Strategy

Pillars 3 and 4 of the mission: lightest, lowest-memory, fastest. This is how we
get there and how we prove it.

## Targets (from `00-VISION.md`)

| Metric | Target |
|--------|--------|
| Cold start → first paint (font cached) | < 150 ms |
| Peak RSS, typical article | < 20 MB |
| Peak RSS, 5 MB HTML page | < 120 MB |
| Relayout on resize, typical page | < 16 ms |
| Idle CPU | ~0% (event-driven; already true) |
| Leaks over 100 navigations | 0 (steady state) |
| Crashes on test corpus / fuzz | 0 |

## Memory levers (in impact order)

1. **Arena allocation** (MISS-007) — collapses thousands of tiny mallocs into a
   few blocks; O(1) teardown; removes per-object allocator overhead. Biggest
   single win.
2. **Stop per-word DOM shredding** (FEAT-011) — a 1000-word article drops from
   ~1000+ nodes to O(structure) nodes, and from ~1000 cached textures to a
   bounded set. Cuts both DOM memory and texture-cache memory dramatically.
3. **String interning** (FEAT-010) — dedupes repeated tag names/classes; enables
   integer compares and content-keyed caches.
4. **Bounded caches** (FIX-011) — texture/image/visited caches get capacity +
   LRU so long sessions reach a steady state instead of growing.
5. **Compact structs** (PERF-003) — pack `LayoutHints` (11 ints → bitfield),
   shrink `DOMNode`, avoid storing redundant strings once typed styles exist.
6. **Input caps** (FIX-015) — hard ceilings prevent pathological blowups.

## Speed levers

1. **Geometric network buffer growth** (FEAT-020) — kills O(n²) body assembly.
2. **curl handle reuse** (FEAT-020) — warm connections/TLS across navigations.
3. **Integer tag classification** (FEAT-010) — removes `strcasecmp` from layout
   hot loops (matters on resize relayout).
4. **Measurement cache** (FIX-008/PERF-004) — cache `TTF_SizeUTF8` results by
   (content,size,style); relayout becomes mostly cache hits.
5. **Content-keyed texture cache** (FIX-011) — survives navigation; identical
   words reuse textures.
6. **Event-driven loop** (already present) — keep it; never busy-wait.
7. **Incremental relayout** (PERF-006, later) — on resize, only reflow; the DOM
   and styles are untouched.

## Measurement methodology

Nothing here is a guess; each claim is backed by a measurement:

- **RSS:** `/usr/bin/time -l` (macOS) / `-v` (Linux) on
  `xs --dump <fixture>`; plus an in-process arena byte counter
  (`arena_bytes`) and a malloc-count hook in tests.
- **Allocations:** a test-only malloc counter wraps `xmalloc`; assert counts for
  a fixture stay within a budget (regression guard).
- **Timing:** `--dump` wall time over a corpus; micro-timers around
  parse/cascade/layout printed under `--profile`.
- **Leaks:** LeakSan over the navigation-loop harness.

See:
- [`memory-budget.md`](memory-budget.md) — per-subsystem byte budgets.
- [`benchmarks.md`](benchmarks.md) — the benchmark harness and baselines.

## Ground rules

- Measure before and after every perf change; record numbers in
  `benchmarks.md`.
- Correctness first: never adopt a perf change that weakens a test or a
  sanitizer run.
- Prefer algorithmic wins (arena, no shredding) over micro-optimizations.
