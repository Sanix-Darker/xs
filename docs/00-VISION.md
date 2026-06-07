# 00 — Vision & Principles

## What xs is

`xs` is a minimal graphical web browser written in portable C. It fetches pages
over HTTP/HTTPS, parses HTML into a DOM, applies CSS, runs a subset of
JavaScript, lays out a box tree, and renders with SDL2 + SDL2_ttf using a
calm, Kindle-like reading typography.

It is **not** trying to be Chrome. It is trying to be the browser you reach for
when you want to read the web with the smallest possible resource footprint:
fast cold start, tiny RSS, no telemetry, no bloat.

## The four pillars (in priority order)

1. **Correctness** — It must not crash, must not corrupt memory, and must show
   the page's content faithfully (text, links, structure, basic styling). A
   browser that segfaults on real-world HTML is worthless. Correctness wins
   over every other concern, always.

2. **Stability** — Long browsing sessions, many navigations, window resizes,
   malformed HTML, huge pages, and hostile input must never destabilize the
   process. No leaks that grow without bound. No use-after-free. Deterministic
   teardown.

3. **Lightness & low memory** — Aggressively minimize heap allocations, peak
   RSS, and per-node overhead. Arena/pool allocation for the DOM. String
   interning. Caps and back-pressure on pathological inputs. Target: render a
   typical article in **under 20 MB RSS**.

4. **Speed** — Fast cold start (<150 ms to first paint on a cached font),
   incremental relayout on resize, O(1) amortized text caching, no needless
   re-parsing. Event-driven render loop (already present) — keep CPU at 0% when
   idle.

When two pillars conflict, the earlier one wins. We never trade correctness for
speed.

## Non-goals (explicitly out of scope)

- Full CSS cascade/specificity engine (we do a pragmatic subset).
- A spec-compliant JavaScript DOM. We expose a small, useful API, not all of
  WHATWG DOM.
- Multi-process/sandboxed architecture.
- Media playback (audio/video), WebGL, WebAssembly.
- Extensions, sync, accounts, telemetry.

## Design tenets

- **Portable C11.** No compiler-specific extensions in our code. Builds with
  clang and gcc, `-Wall -Wextra -pedantic` clean (our code; vendored libs
  excepted).
- **Own your memory.** Every allocation has a clear owner and a clear free
  path. Prefer arenas for tree-shaped lifetimes.
- **Fail safe.** On any parse/fetch/render error, degrade gracefully — show an
  error page, never crash.
- **Treat all network input as hostile.** Bound every loop, validate every
  length, never trust a `Content-Length` or a tag name.
- **Test everything testable.** Pure logic (parsing, CSS, layout math, URL
  resolution) is unit-tested headlessly. Rendering is smoke-tested.
- **Measure, don't guess.** Performance claims are backed by a benchmark or a
  sanitizer/heap profile.

## Definition of "fully feature-proof"

For this project, that means:

- Renders the top ~50 real-world reading sites (news, docs, wikis, blogs)
  without crashing and with legible output.
- Handles malformed/adversarial HTML, huge documents, deep nesting, and broken
  encodings without crashing or unbounded memory growth.
- Has a CI-runnable headless test suite covering parser, CSS, layout, URL,
  history, and JS-bridge logic.
- Has a documented, reproducible build on Linux and macOS.
- Clean under ASan/UBSan/LeakSanitizer on the test corpus.

## Success metrics (tracked in `performance/`)

| Metric | Target |
|--------|--------|
| Cold start to first paint | < 150 ms (font cached) |
| Peak RSS on a typical article | < 20 MB |
| Peak RSS on a huge page (5 MB HTML) | < 120 MB |
| Relayout on resize (typical page) | < 16 ms |
| Idle CPU | ~0% (event-driven) |
| Leaks on navigate ×100 | 0 bytes growth (steady state) |
| Crashes on the test corpus | 0 |
