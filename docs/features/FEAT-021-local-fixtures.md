# FEAT-021 — `file://` loading + test fixtures

- **Priority:** P1 (unblocks deterministic full-pipeline testing)
- **Files:** `network.c` (or `main.c`), `tests/fixtures/`

## Why

Every integration/golden test needs deterministic input that does not depend on
the network. Loading local files via `file://` (or a bare path) lets the test
suite run the **entire** pipeline (fetch→parse→css→layout→dump) offline and in
CI.

## Design

1. `fetch_url` recognizes `file://` and bare local paths: read the file into the
   `FetchResult` body, set `status=200` (synthetic), `content_type` guessed from
   extension (`.html/.htm`→text/html, `.css`→text/css, `.txt`→text/plain),
   `final_url` = the absolute `file://` form (for relative resolution of
   `file://` links and `<link>`/`<img>`).
2. Respect the size cap.
3. `resolve_url` already handles relative paths for http; extend it (or its
   `file://` cousin) so relative `href`/`src` resolve against a `file://` base
   directory.

## Fixtures to ship under `tests/fixtures/`

- `basic.html` — headings, paragraphs, a link, a list.
- `whitespace.html` — tabs/newlines/multiple spaces (FIX-003).
- `script.html` — a `<script>` with whitespace-significant JS (FIX-002).
- `css_basic.html` — inline `<style>` with class/id/group selectors (FEAT-004).
- `css_inline.html` — `style=""` attributes (FEAT-013).
- `unknown_tags.html` — `<svg>`, custom elements (FIX-001).
- `nested.html` — deeply nested divs (FIX-015, capped).
- `entities.html` — HTML entities and UTF-8 text.
- `align.html` — text-align center/right (FEAT-002).
- `table.html`, `img.html` — for MISS-005/MISS-002 once implemented.

## Micro-plan

1. Implement `file://`/path loading in the fetch layer.
2. Extension→content-type guesser.
3. Author fixtures; keep them small and stable.
4. Wire `--dump` (FEAT-003) + goldens in `tests/golden/`.

## Acceptance tests

- `xs --dump file://$PWD/tests/fixtures/basic.html` matches
  `tests/golden/basic.txt`.
- Reading a missing file yields the error-document path, not a crash.
- The size cap rejects an over-large local file (inject small cap).

## Risk

Low. Self-contained and high leverage; do it right after `--dump`.
