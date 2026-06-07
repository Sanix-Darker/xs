# xs

A minimal graphical web browser written in C. Fetches web pages over HTTP(S),
parses HTML into a DOM, applies a pragmatic CSS cascade, runs a small subset of
JavaScript, lays out a box tree, and renders with SDL2 + SDL2_ttf using a calm,
Kindle-like reading typography. It aims to be the strongest, most stable,
lightest, lowest-memory reading browser it can be.

> Engineering docs (vision, audit, roadmap, per-feature specs, testing &
> performance strategy, living status) live in [`docs/`](docs/README.md).

## Architecture

```
URL ─ network.c (libcurl / file://) ─ FetchResult{body,status,content-type,charset}
    ─ encoding.c (→ UTF-8) ─ parser.c (Gumbo → DOM, attrs, parent ptrs, tag ids)
    ─ css.c + select.c + style.c (UA + author cascade, specificity, inheritance)
    ─ javascript.c (MuJS: console/document) ─ layout.c (box tree, tables, images)
    ─ render.c + ui.c (SDL2: themes, scrollbar, find, history)
```

Core modules:

- **network.c** — HTTP(S) fetch via libcurl + `file://`/local loading, size
  caps, geometric buffer growth, status/content-type/charset/final-URL.
- **encoding.c** — charset detection (header / `<meta>` / BOM / UTF-8 validity)
  and Latin-1 / Windows-1252 → UTF-8 transcoding.
- **parser.c** — Gumbo → `DOMNode` tree with bounded-copy tag names (safe on
  unknown/SVG/custom elements), generic attribute capture, parent pointers, and
  interned tag ids (`tagid.c`).
- **css.c / select.c / style.c** — comment-aware CSS parser, selector engine
  (type/universal/class/id/compound/grouping/descendant) with specificity, a
  UA + author cascade with inheritance, and typed colors/lengths.
- **document.c** — the single pipeline (parse → CSS → scripts → text-split) used
  by both the CLI and the interactive renderer.
- **javascript.c** — MuJS host with `console.*`, `navigator`, and a read-only
  `document` (`title`, `getElementById` → `getAttribute`/`textContent`).
- **layout.c / table.c** — box layout with headings, lists, blockquotes,
  `text-align`, margin/padding spacing, simple table grids, and `<img>` boxes.
  Text is measured at true font size via a `FontProvider` callback.
- **render.c / ui.c** — SDL2 rendering with font cache (size/bold/italic, bold &
  italic synthesized), bounded texture cache, themes (kindle/light/dark/sepia),
  scrollbar, find-in-page, visited links, history, and text zoom.
- **arena.c / util.c** — bump allocator and safe allocation/ctype helpers.

## Dependencies

- **libcurl** — HTTP fetching
- **SDL2** + **SDL2_ttf** — Window, rendering, font rasterization
- **DejaVu Sans** font — `DejaVuSans.ttf` (bold/italic are synthesized, so a
  dedicated bold/italic TTF is not required). Searched in exe dir, cwd, and
  common system font directories.
- **CMake** >= 3.10

### Ubuntu/Debian

```sh
sudo apt install build-essential cmake libcurl4-openssl-dev libsdl2-dev libsdl2-ttf-dev fonts-dejavu-core
```

### macOS (Homebrew)

```sh
brew install cmake sdl2 sdl2_ttf curl
```

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

## Tests

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Usage

```sh
./build/xs <url|file|path>

# examples
./build/xs https://example.com
./build/xs file://$PWD/tests/fixtures/basic.html
```

### CLI flags (headless / debugging)

```sh
./build/xs --dump  --width=800 <url>   # print layout boxes, then exit
./build/xs --dump=tree         <url>   # print the DOM tree, then exit
./build/xs --profile           <url>   # print fetch/build/layout timings + counts
./build/xs --version
./build/xs --help
```

`--dump` is the backbone of the golden test suite (deterministic, no window).
See [`docs/testing/dump-format.md`](docs/testing/dump-format.md).

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `/` | Focus search/URL bar |
| `Enter` | Navigate to URL or search (or next match in find mode) |
| `Escape` | Unfocus search bar / close find |
| `j` / `Down` | Scroll down |
| `k` / `Up` | Scroll up |
| Mouse wheel | Scroll |
| `Alt+Left` | Back |
| `Alt+Right` | Forward |
| `r` | Reload current page |
| `t` | Cycle theme (kindle / light / dark / sepia) |
| `Ctrl/Cmd+F` | Find in page (type to filter, `Enter` cycles matches) |
| `Ctrl/Cmd` `+` / `-` / `0` | Zoom text in / out / reset |
| Click link | Navigate |
| Click submit | Submit a GET form |

## Features

- HTML parsing via Gumbo with memory-safe handling of unknown/SVG/custom tags
- Generic attribute capture; parent pointers; interned tag ids
- Character-encoding detection + Latin-1 / Windows-1252 → UTF-8 transcoding
- CSS: comment-aware parser; type/universal/class/id/compound/grouping/descendant
  selectors with specificity; UA + author cascade; inheritance; inline `style=`;
  external `<link rel=stylesheet>`; typed colors and lengths (px/%/em/pt)
- Heading hierarchy, bold/italic (synthesized), lists, blockquotes, `<hr>`,
  `<pre>`/`<code>`, `text-align`, margin/padding spacing, block backgrounds
- Simple table grid layout; `<img>` boxes (sized by attributes) with alt
  placeholders
- Word-level wrapping measured at the true font size, with reflow on resize
- JavaScript via MuJS with `console.*`, `navigator`, and a read-only `document`
  (`title`, `getElementById` → `getAttribute`/`textContent`)
- Themes, scrollbar, page title in the window, find-in-page, visited-link
  coloring, text zoom, and back/forward history
- Resource caps (document size, DOM depth, node count) and a bounded texture
  cache for stability under hostile/large input
- Font cache (size + bold + italic) and texture cache for performance

## Notes

- JavaScript is intentionally a small, mostly read-only host — it is not a
  spec-compliant DOM. `document.write` and layout-affecting DOM mutation are not
  supported.
- Image pixel decoding is gated on SDL2_image (optional, off by default); `<img>`
  otherwise reserves a sized placeholder box with alt text.
- Interactive text-input editing inside forms is not yet implemented; form GET
  submission (gathering named fields into a query string) is.
- See [`docs/STATUS.md`](docs/STATUS.md) for the precise state of every feature
  and [`docs/ROADMAP.md`](docs/ROADMAP.md) for what remains.

