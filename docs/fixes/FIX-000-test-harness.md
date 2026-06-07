# FIX-000 — Broken test target / no tests exist

- **Severity:** P1 (process/quality blocker)
- **Files:** `CMakeLists.txt`, new `tests/`

## Problem

`CMakeLists.txt` declares:

```cmake
add_executable(xs_tests
    tests/test_core.c
    parser.c css.c layout.c javascript.c mujs/one.c ${GUMBO_SOURCES})
add_test(NAME xs_tests COMMAND xs_tests)
```

But `tests/test_core.c` does not exist. So:
- `cmake --build build` fails on the test target.
- The README's documented `ctest` workflow cannot run.
- There is **no automated coverage at all**, so every other fix is unverifiable.

Additionally, `xs_tests` links `layout.c` (which `#include`s `SDL2/SDL_ttf.h`
and calls `TTF_SizeUTF8`). Tests must be runnable **headless** (no display), so
layout's text measurement must degrade gracefully when `font==NULL` (it already
falls back to an approximation — good — but we must never require a window in
tests).

## Evidence

```
$ ls tests
ls: tests: No such file or directory
```

## Micro-plan

1. Create `tests/` with a tiny zero-dependency assertion harness
   (`tests/test_util.h`): `CHECK`, `CHECK_EQ_INT`, `CHECK_STR_EQ`, a test
   registry, and a `main` that runs all and prints a summary + non-zero exit on
   failure.
2. Create `tests/test_core.c` as the aggregator that includes per-area test
   files, OR keep separate test executables per area. Decision: **one test
   binary, multiple translation units** registered via constructor list, to
   keep `ctest` simple. Implementation: a manual registration table in
   `test_main.c`.
3. Ensure the test binary does **not** require SDL video. It may link SDL2_ttf
   for measurement but must run with `font=NULL` paths and never call
   `SDL_Init(SDL_INIT_VIDEO)` or open a window.
4. Make `CMakeLists.txt` test target compile only what is needed and add
   sanitizer flags behind an option (`-DXS_SANITIZE=ON`).
5. Add a `ctest` smoke that runs `xs --dump file://tests/fixtures/basic.html`
   (depends on FEAT-021 local file loading + `--dump`).

## Acceptance tests

- `cmake -S . -B build && cmake --build build -j && ctest --test-dir build`
  exits 0 with all tests reported.
- The test binary runs with no `DISPLAY` set.

## Risk

Low. Pure additive tooling. Sanitizer option must be off by default to keep
normal builds fast.
