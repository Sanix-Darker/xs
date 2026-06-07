# FEAT-003 — CLI flags: `--dump`, `--width`, `file://`, `--version`

- **Priority:** P2 (unblocks deterministic testing — high leverage)
- **Files:** `main.c`, `network.c` (file://), `layout.c`

## Why

There is currently no way to run the browser headlessly or deterministically.
`--dump` (render the layout to text/coordinates on stdout and exit) is the
single most valuable testing tool: it lets golden tests assert exact layout
without a display.

## Design

```
xs [options] <url|file|path>

Options:
  --dump            Lay out the page and print boxes as text, then exit 0.
  --dump=tree       Print the DOM tree instead of layout boxes.
  --width=N         Viewport width for layout/dump (default 950).
  --height=N        Viewport height (default 700).
  --version         Print version and exit.
  --help            Print usage and exit.
```

- A bare path or `file://...` loads from disk (FEAT-021).
- `--dump` output format (stable, documented):
  ```
  BOX x y w h font=NN flags=BIHL... tag=<name> text="..."
  ```
  Coordinates are pre-scroll, pre-search-bar (raw layout space) for stability.

## Micro-plan

1. Parse argv into an `Options` struct (simple hand-rolled loop; no getopt
   dependency for portability).
2. `--dump` path: `fetch/file → build_document → layout_dom(doc, NULL, width)`
   (NULL font → headless measurement) → print boxes → exit. No SDL init.
3. Document the dump format in `docs/testing/` so golden files are stable.

## Acceptance tests

- `xs --dump --width=800 file://tests/fixtures/basic.html` prints a deterministic
  box list; a golden file in `tests/golden/basic.txt` matches.
- `xs --version` prints a version string and exits 0.
- `--dump` runs with no `DISPLAY`.

## Risk

Low. Additive. The dump format must stay stable once goldens exist — version it
if it must change.
