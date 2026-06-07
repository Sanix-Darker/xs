# Testing Strategy

Correctness is the top pillar, so testing is not optional. Everything that can
be tested headlessly is tested headlessly, and rendering gets smoke coverage.

## Layers

1. **Unit tests** — pure logic, no SDL window, no network:
   - arena (alloc/align/destroy/accounting)
   - strpool (intern identity), tag id mapping
   - HTML parse (tag names incl. unknown/SVG, attributes, text)
   - whitespace/text iterator (collapse, pre, trailing-space)
   - CSS tokenizer/parser (comments, declarations)
   - selectors (type/class/id/group/descendant/universal)
   - cascade (specificity, inheritance, origin order)
   - color/length parsing, box-model shorthand expansion
   - layout math (wrapping, text-align shift, table width distribution)
   - URL resolution (absolute/root-relative/relative/file)
   - history (push/back/forward/truncate)
   - encoding transcode (latin1/1252→utf8, detection order)
   - network helpers (content-type/charset parse, buffer growth cap)
   - JS bindings (console capture, getElementById, error message)
   - texture-cache keying (content hash, eviction)
   - find-in-page match/scroll, visited set, text-edit (caret) ops

2. **Golden tests** — full pipeline via `--dump` on `file://` fixtures.
   Deterministic box lists compared against `tests/golden/*.txt`. These catch
   regressions in layout/cascade end-to-end.

3. **Smoke tests** — the real binary runs `--dump` over every fixture and must
   exit 0 (no crash) with non-empty output. Optionally run a tiny set of live
   URLs behind an env guard (`XS_LIVE=1`).

4. **Sanitizer runs** — the unit + golden suite under
   ASan+UBSan and LeakSan (`-DXS_SANITIZE=ON`). Zero reports required.

5. **Fuzz (P2)** — a libFuzzer/AFL harness over `document_from_html` on random/
   mutated HTML to flush crashes and OOMs (with the input caps from FIX-015).
   See `fuzzing.md`.

## Harness

- Tiny header-only assert lib (`tests/test_util.h`): `CHECK`, `CHECK_EQ_INT`,
  `CHECK_STR_EQ`, `CHECK_NEAR`, plus a registry. One `xs_tests` binary runs all
  registered tests, prints `PASS/FAIL` per test and a summary, exits non-zero on
  any failure. `ctest` wraps it.
- A separate `xs_dump` smoke driver (or `xs --dump`) is invoked by `ctest` over
  fixtures.
- Tests must run with no `DISPLAY`. Layout uses the `font==NULL` measurement
  path (deterministic) so golden coordinates are stable across machines.

## Determinism rules

- Golden coordinates use the headless measurement model (no system fonts), so
  they are identical everywhere. A separate, non-golden visual check uses real
  fonts.
- No test depends on wall-clock time, network, or locale.

## CI workflow (target)

```
configure (cmake) → build (all targets) → ctest (unit+golden+smoke)
→ sanitizer build → ctest → (optional) fuzz short run
```

See:
- [`test-matrix.md`](test-matrix.md) — which test maps to which FIX/FEAT/MISS.
- [`dump-format.md`](dump-format.md) — the stable `--dump` output contract.
- [`fuzzing.md`](fuzzing.md) — fuzz harness plan.
