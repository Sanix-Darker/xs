# FEAT-014 — `<link rel="stylesheet">` external CSS

- **Priority:** P2
- **Files:** `document.c`, `network.c`, `css.c`

## Why

Most sites put their CSS in external files via `<link rel="stylesheet"
href="...">`, not inline `<style>`. Without fetching these, author styling is
largely absent.

## Design

1. After building the DOM, collect all `<link rel="stylesheet" href="...">`
   (and `@import` — P3) with their resolved absolute URLs (base = final fetched
   URL, FIX-010).
2. Fetch each (subject to the size cap and a per-page stylesheet count cap, and
   only `text/css`-ish responses).
3. Parse each into the same stylesheet structure; concatenate in document order
   **before** inline `<style>` (author order: external link, then later inline,
   per source position — approximate by DOM order of the `<link>`/`<style>`
   nodes).
4. Cascade as in FEAT-005.

Caching: a simple in-memory URL→stylesheet cache avoids refetching shared CSS on
navigation within a session (bounded, LRU).

## Micro-plan

1. Requires attribute capture (FEAT-013) for `rel`/`href`/`type`.
2. `document_collect_stylesheet_links()` returns resolved URLs in DOM order.
3. Fetch + parse each; handle failures gracefully (skip, keep going).
4. Order-preserving merge with inline `<style>` by walking the DOM once and
   appending each stylesheet source as encountered.
5. Network: synchronous fetch for now; respects caps. (Async later, FEAT-022.)

## Acceptance tests

- Unit: link collection returns resolved absolute URLs for relative `href`s
  against a base.
- Integration (local fixtures + `file://`): a fixture linking
  `file://.../site.css` applies its rules (golden `--dump` shows the styled
  result).
- A failed stylesheet fetch does not abort the page.

## Risk

Medium. Multiple synchronous fetches add latency; mitigated by the session
cache and caps. Ordering correctness is the subtle part — lock it with a golden.
