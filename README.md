# xs

A minimal graphical web browser written in C. Fetches web pages over HTTPS, parses HTML into a DOM, applies CSS, runs basic JavaScript, and renders with SDL2 using Kindle-like typography.

## Architecture

```
URL -> network.c (libcurl) -> parser.c (Gumbo HTML) -> css.c (stylesheet)
    -> javascript.c (MuJS) -> layout.c (box layout) -> render.c (SDL2 + TTF)
```

- **network.c** — HTTP/HTTPS fetch via libcurl
- **parser.c** — HTML parsing with Gumbo, DOM tree construction, word splitting for wrapping
- **css.c** — Naive CSS parser, stylesheet application to DOM nodes
- **javascript.c** — `<script>` execution via MuJS (no DOM/browser APIs)
- **layout.c** — Box layout engine with context-based font sizing, heading hierarchy, list markers, blockquote indents, wireframe borders for structural elements
- **render.c** — SDL2 rendering with font cache (size/bold), texture cache, Kindle-style warm background, link underlines, list bullets/numbers, wireframe overlays

## Dependencies

- **libcurl** — HTTP fetching
- **SDL2** + **SDL2_ttf** — Window, rendering, font rasterization
- **DejaVu Sans** fonts — `DejaVuSans.ttf` and `DejaVuSans-Bold.ttf` (searched in exe dir, cwd, `/usr/share/fonts/truetype/dejavu/`)
- **CMake** >= 3.10

### Ubuntu/Debian

```sh
sudo apt install build-essential cmake libcurl4-openssl-dev libsdl2-dev libsdl2-ttf-dev fonts-dejavu-core
```

## Build

```sh
mkdir -p build && cd build
cmake ..
make
```

## Tests

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Usage

```sh
./xs <url>
```

```sh
./xs https://example.com
./xs https://en.wikipedia.org/wiki/C_(programming_language)
```

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `/` | Focus search/URL bar |
| `Enter` | Navigate to URL or search |
| `Escape` | Unfocus search bar |
| `j` / `Down` | Scroll down |
| `k` / `Up` | Scroll up |
| Mouse wheel | Scroll |
| `Alt+Left` | Back |
| `Alt+Right` | Forward |
| Click link | Navigate |

## Features

- HTML parsing via Gumbo (handles real-world HTML)
- Heading hierarchy (h1-h6) with proportional font sizes
- Bold/italic context propagation through inline elements
- Ordered and unordered list markers (bullets and numbers)
- Link detection, underlines, and click navigation
- Blockquote indentation with left bar
- `<hr>` horizontal rules
- `<pre>`/`<code>` background shading
- Wireframe borders on structural elements (div, section, article, nav, header, footer)
- CSS `font-size`, `width`, `height`, `background`, `text-align` property parsing/storage support
- Word-level text wrapping with reflow on resize
- Warm off-white background (Kindle-style)
- Font cache (size + bold variant) and texture cache for performance
- Back/forward navigation history
- URL bar with search fallback to Google

## Notes

- JavaScript execution does not expose DOM APIs (`document`, `window`, events).
- `text-align` is parsed/stored in computed style, but not yet applied by layout/rendering.
