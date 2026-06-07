# Fuzzing Plan

- **Priority:** P2 (after the core pipeline is hardened)

## Goal

Flush crashes, heap errors, and unbounded memory growth in the parsing/styling/
layout pipeline by feeding it random and mutated HTML/CSS — the exact kind of
hostile input a browser faces.

## Targets

1. `document_from_html(data, len, base, charset)` — the highest-value target;
   covers parser, attributes, CSS extraction, cascade.
2. `parse_css(css_text)` — CSS tokenizer/parser in isolation.
3. `layout_dom(doc, NULL, width)` — layout on the resulting tree (headless).
4. `latin1/cp1252/utf8` transcoders and `detect_charset`.

## Harness

A libFuzzer entry point (compiled with `-fsanitize=fuzzer,address,undefined`):

```c
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > XS_MAX_DOCUMENT_BYTES) return 0;
    Document* d = document_from_html((const char*)data, size, "file:///fuzz", NULL);
    if (d) {
        Layout* l = layout_dom(d, NULL, 800);
        free_layout(l);
        document_free(d);
    }
    return 0;
}
```

Build behind `-DXS_FUZZ=ON` so it does not affect normal builds. If libFuzzer is
unavailable (no clang fuzzer), provide an AFL-style `main` reading stdin.

## Invariants the fuzzer enforces

- No ASan/UBSan reports (no OOB, no UAF, no signed overflow, no bad ctype).
- No leaks across iterations (arena destroyed each run → LeakSan clean).
- Respects caps: depth ≤ `XS_MAX_DEPTH`, nodes ≤ `XS_MAX_NODES`, bytes ≤
  `XS_MAX_DOCUMENT_BYTES` (no OOM on the cap).
- Terminates: bounded loops everywhere (no hang on pathological input).

## Corpus

Seed with the `tests/fixtures/*.html` files plus a handful of real-world saved
pages (small). Minimize with `-merge=1`.

## Triage workflow

Any crash → minimize (`-minimize_crash`) → add the minimized input as a
regression fixture under `tests/fixtures/crash/` → fix → keep the fixture in the
golden/smoke set forever.

## Run cadence

- Short smoke (60s) in CI on each change.
- Longer soak (hours) periodically, locally or in a nightly job.
