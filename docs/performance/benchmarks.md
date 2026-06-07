# Benchmarks

A small, reproducible benchmark harness and a running log of measured numbers.
Every performance-relevant change records before/after here.

## Harness

`xs --dump` is the benchmark workload (full pipeline, no window). Optional
`--profile` prints phase timings:

```
PROFILE fetch=<ms> parse=<ms> css=<ms> layout=<ms> total=<ms> nodes=<n> arena=<bytes>
```

Scripts (to be added under `tools/`):

- `tools/bench.sh <fixture> [width]` — runs `--dump --profile` N times, reports
  median phase timings and RSS via `/usr/bin/time`.
- `tools/memcount` — builds the test binary with the malloc-count hook and
  prints allocations + arena bytes per fixture.

## Workloads

| Name | Source | Size | Purpose |
|------|--------|-----:|---------|
| basic | fixture | tiny | sanity, cold start |
| article | fixture (~1500 words) | ~50 KB | typical reading page |
| wiki | saved Wikipedia article | ~500 KB | heavy real-world |
| huge | generated | 5 MB | stress / caps |
| nested | generated deep divs | small | depth cap |

## Baseline (post-overhaul, measured)

Captured on macOS/arm64 (Apple clang), `--profile` + `/usr/bin/time -l`, font
cached, headless `--dump`.

| Workload | Body | nodes | boxes | build ms | layout ms | total ms | peak RSS |
|----------|-----:|------:|------:|---------:|----------:|---------:|---------:|
| basic    | 393 B | 46 | 43 | 0.25 | 0.01 | 0.57 | ~5 MB |
| table    | 298 B | 32 | 20 | 0.45 | 0.01 | 0.77 | ~5 MB |
| article (300 p, ~12k words), pre-FEAT-011 | 70 KB | 12309 | 12303 | 19.8 | 2.0 | 22.2 | 14.5 MB |
| article (300 p, ~12k words), post-FEAT-011 | 70 KB | **608** | 12303 | **6.3** | 0.8 | **7.6** | 14.0 MB |

Observations:
- **FEAT-011 (layout-time word breaking) cut the article's DOM node count
  20× (12309 → 608)** and total time ~3× (22 → 7.6 ms) by no longer creating a
  DOM node per word. Word boxes are transient layout output, not persistent DOM.
- A typical article renders in ~7.6 ms total and **~14 MB RSS — under the
  20 MB target**.
- Layout itself is fast (~0.8 ms) thanks to the flat box array and binary-search
  tag classification.
- Remaining RSS is dominated by SDL/TTF/curl runtime and the font atlas; the
  arena (MISS-007) would further shrink DOM/string allocation overhead.

## Baseline (pre-overhaul, git b09f660)

Not separately captured; the pre-overhaul build also shredded text into
per-word nodes and additionally crashed on unknown tags (FIX-001), so a direct
comparison is unsafe. The numbers above are the post-correctness baseline that
future perf work (FEAT-011/MISS-007) must beat.

## Running log

| Date | Change | Workload | RSS | total ms | nodes | mallocs | Notes |
|------|--------|----------|----:|---------:|------:|--------:|-------|
| _tbd_ | baseline capture | all | | | | | first measurement |

## How to read this

- **RSS** is peak resident set from `/usr/bin/time`.
- **total ms** is median of N `--dump` runs (font cached).
- **nodes** is DOM node count; **mallocs** is allocation count from the hook.
- A change that improves one metric must not regress another beyond noise
  without justification recorded in the Notes column.
