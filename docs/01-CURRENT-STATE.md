# 01 — Current State Audit

An honest, file-by-file assessment of the code as it exists at the start of this
overhaul (git `b09f660`). This is the baseline every fix and feature is measured
against.

## Build status

- Builds cleanly via direct `cc` invocation with all deps present.
- **Our code emits zero warnings** under `-Wall -Wextra -std=c11`. All warnings
  come from vendored `mujs/` and `gumbo_src/`.
- `CMakeLists.txt` defines a test target `xs_tests` that compiles
  `tests/test_core.c` — **but that file does not exist**, so `cmake` configure
  succeeds but `cmake --build` for the test target fails. The documented
  `ctest` workflow in the README is therefore broken.

## Dependencies

| Dependency | Role | Status |
|------------|------|--------|
| libcurl | HTTP(S) fetch | present |
| SDL2 | window + render | present |
| SDL2_ttf | font rasterization | present |
| Gumbo (vendored) | HTML parsing | present in `gumbo_src/` |
| MuJS (vendored) | JS engine | present in `mujs/` |
| DejaVuSans.ttf | regular font | present |
| **DejaVuSans-Bold.ttf** | bold font | **MISSING** — only `DejaVuSans.ttf` and `DejaVuSerif.ttf` ship |
| duk_config.h | (Duktape config?) | present but **unused** — no Duktape in build; dead file |

## Pipeline (as built)

```
main.c
  network_init
  fetch_url            (network.c, libcurl)
  parse_html           (parser.c, Gumbo -> DOMNode tree)
  split_text_nodes     (parser.c, splits text into per-word nodes)
  extract_style_text   (parser.c, concatenates <style> text)
    parse_css          (css.c)
    apply_stylesheet_to_dom (css.c)
  run_scripts_in_dom   (javascript.c, MuJS, no DOM bindings)
  render_layout        (render.c, owns event loop; calls layout_dom)
  network_cleanup
```

## File-by-file findings

### main.c (63 lines)
- Linear happy-path pipeline. Reasonable.
- **Bug:** runs `split_text_nodes` **before** `run_scripts_in_dom`. Because
  `split_text_nodes` shreds every `#text` node containing a space into many
  one-word `#text` nodes, the JS source inside `<script>` is destroyed
  (whitespace removed, statements concatenated) before MuJS sees it. See
  FIX-002.
- No way to load a local file or read from stdin (testing/debug friction).

### network.c (74 lines)
- Single blocking `curl_easy_perform` into a growing buffer. Fine for a start.
- Sets a desktop UA, follows redirects (max 5), enables gzip, 30s timeout.
- **Missing:** no `Content-Type`/charset capture (needed for encoding + to
  distinguish HTML from binary). No max-size cap (a malicious/huge response can
  exhaust memory). No HTTP status code surfaced (404 vs 200 indistinguishable).
  No connection reuse across navigations (new handle each time). See
  FIX-010, FEAT-020.
- `realloc` growth is linear (`size + chunk`), causing O(n^2) copying on large
  bodies. Should grow geometrically. See PERF item.

### parser.c (205 lines)
- Builds a `DOMNode` tree from Gumbo. Children stored in a growable array.
- **P0 Bug:** for `GUMBO_TAG_UNKNOWN`, it does
  `tag_name = element->original_tag.data` then `strdup(tag_name)`.
  `original_tag` is a `GumboStringPiece` whose `.data` is **not
  NUL-terminated** (it points into the source buffer). `strdup` reads past the
  tag, copying arbitrary following bytes as the tag name — heap over-read +
  garbage node names. See FIX-001.
- **Bug:** `create_dom_node` does not check `calloc` for NULL; a failed
  allocation dereferences NULL. See FIX-005.
- **Does not capture attributes** other than `href` on `<a>`. No `class`, `id`,
  `src`, `style`, `alt`, `title`, etc. This blocks CSS class/id selectors,
  images, inline styles. See MISS-001.
- `split_text_nodes`:
  - Collapses all runs of spaces and **drops tabs/newlines entirely** (only
    splits on `' '`, treats `\n`/`\t` as part of a "word"), so whitespace
    handling is wrong for real HTML. See FIX-003.
  - Splitting into one node per word massively inflates node count and heap
    usage (a 1000-word article becomes 1000+ DOM nodes plus 1000 cached
    textures). Major memory/perf problem. See PERF-002 / FEAT-011.
  - Mutates the tree in a way that is hard to reason about and re-frees nodes.
- No DOCTYPE / comment handling distinctions (comments are simply dropped by
  Gumbo walk — acceptable).

### parser.h (29 lines)
- `DOMNode` has no attribute map, no parent pointer, no tag-id (uses string
  compares everywhere). Adding a parent pointer + interned tag id would speed
  up layout and enable selector matching. See FEAT-010.
- `ComputedStyle` is a handful of raw strings (`width`, `height`, `background`,
  `text_align`, `font_size`) — re-parsed as strings at layout time. Should be
  resolved to typed values. See FEAT-005.

### css.c (172 lines)
- Naive but functional flat parser: `selector { prop: val; ... }`.
- **Bug:** does not skip CSS comments `/* ... */`; a comment containing `{` or
  `}` desynchronizes the parser. See FIX-006.
- **Bug:** `trim()` calls `isspace(*str)` with a `char` that may be negative on
  signed-char platforms → UB. Must cast to `unsigned char`. See FIX-004.
- **Bug:** no `malloc`/`realloc` NULL checks anywhere. See FIX-005.
- **Limitation:** selector matching is **tag-name only** and **case-insensitive
  exact**. No class (`.x`), id (`#x`), descendant, grouping (`a, b`),
  universal (`*`), or attribute selectors. Comma-separated selectors are
  treated as one opaque selector string and never match. See FEAT-004.
- **Limitation:** no specificity, no cascade order beyond "last rule wins per
  property", no inheritance, no `!important`, no shorthand expansion
  (`margin`, `padding`, `font`, `border`). `background` only stores the raw
  string; not parsed to a color. See FEAT-005.
- Only 5 properties are stored; everything else is parsed then discarded.
- No support for `<link rel=stylesheet>` external CSS or inline `style=""`
  attributes (the latter also blocked by parser not capturing attributes).

### javascript.c (74 lines)
- Creates a MuJS state, finds `<script>` nodes, concatenates their `#text`
  children, evaluates with `js_dostring`.
- **Broken in practice** because of FIX-002 (text already shredded) — the
  concatenated source is mangled.
- **No browser/DOM bindings at all**: no `document`, `window`, `console`,
  `setTimeout`, etc. Even `console.log` does nothing. So script side effects
  are invisible and useless. See MISS-003.
- Runs scripts **once at load**, with no connection to layout (DOM mutations
  from JS, even if APIs existed, would not reflow). Acceptable for now but
  documented.
- `<script src="...">` external scripts are ignored (no attribute capture, no
  fetch). See MISS-003.
- A single broken script prints a generic error and continues — fine, but the
  error message is not useful (no line/message from MuJS surfaced).

### layout.c (460 lines)
- Single-pass recursive layout producing a flat array of `LayoutBox`. Threads a
  `LayoutContext` (cursor, font, list state, href) through recursion.
- Handles: block vs inline classification (binary search over sorted tables),
  headings h1–h6 with sizes, `<br>`, `<hr>`, lists (`ul`/`ol` markers),
  blockquote indent, `<pre>`/`<code>`, bold/italic context, structural
  wireframe borders, word-level wrapping.
- **Bug:** `text-align` is parsed/stored but **never applied** (README admits
  this). Centered/right text renders left. See FEAT-002.
- **Bug:** width from CSS is read as an integer via `strtol`, silently ignoring
  units and `%` (`width: 50%` becomes `50`px). See FIX-007 / FEAT-005.
- **Limitation:** no real box model — `margin`/`padding`/`border-width` from CSS
  are ignored; spacing is hardcoded constants. See FEAT-006.
- **Limitation:** inline wrapping measures each word separately and adds a fixed
  `INLINE_GAP`; this diverges from actual space width and accumulates error;
  justification/kerning absent. Acceptable but noted. See FEAT-011.
- **Limitation:** no tables layout (table tags are treated as generic blocks,
  cells stack vertically). See MISS-005.
- **Limitation:** no images (`<img>` is in the inline table but produces no box
  / no fetch / no decode). See MISS-002.
- **Bug risk:** `measure_text_width` scales a 16pt measurement by
  `target_size/16` instead of measuring at the real size — produces wrong
  widths for headings, causing mis-wrapping. Should measure with the actual
  font size (the render side already loads sized fonts). See FIX-008.
- `layout_dom` defaults width to 800 if `window_w<=0`; fine.

### layout.h (60 lines)
- `LayoutHints` is a struct of ints — clear but heavy (11 ints per box). Could
  be a bitfield to shrink per-box memory; minor. See PERF-003.

### render.c (777 lines)
- SDL2 event loop, search/URL bar, scrolling, history, font cache, texture
  cache, link hit-testing, error DOM, resize relayout. This is the most
  complete module.
- **Leak/lifetime bug:** in the resize handler it detaches `dom` from the old
  layout, frees the layout, relays out — but on the very first error path and
  in `navigate_to`/back/forward it calls `free_layout` which **frees the DOM**.
  The initial `dom` passed to `render_layout` is also freed at the end. Paths
  are mostly consistent but fragile; need an explicit ownership model. See
  FIX-009.
- **Bug:** texture cache is keyed by the **text pointer** (`b->node->text`).
  After navigation the DOM (and those pointers) are freed and new nodes may be
  allocated at the same address → stale cache hits rendering wrong text. The
  cache is cleared on navigate/resize which mostly masks this, but it is
  pointer-identity fragile. Should key by interned string id or content hash.
  See FIX-011 / PERF-004.
- **Bug:** `load_font_path` builds a `paths[6]` array but can write up to 5
  entries; fine, but `cwd_font` is `[256]` while `exe_font` is `[1024]` — long
  cwd paths silently truncate. Minor. See FIX-012.
- **Missing affordances:** no scrollbar, no loading indicator, no status text,
  no title from `<title>`, no reload key, no copy, no find-in-page,
  no clickable URL bar editing position (caret), no text selection. See
  FEAT-007/008/009.
- Font fallback only tries DejaVu + one Liberation path; bold falls back to
  regular (because the bold TTF is missing). Italic is never synthesized
  (SDL_ttf style flags unused) so `<em>`/`<i>` look identical to regular. See
  FIX-013.
- Search bar always starts focused (`search_focused=true`) and the displayed
  text logic is okay, but there is no caret and Backspace handling edge cases.
- The wireframe borders on every structural div add visual noise on real sites
  (every container boxed). It is a deliberate "wireframe" aesthetic but should
  be toggleable. See FEAT-007.

### tag_tables.h (17 lines)
- Sorted block/inline tables for binary search. `<img>` is listed inline but
  unused for rendering. Tables are reasonable. `<title>` not excluded here
  (excluded in layout_node directly).

### duk_config.h (3804 lines)
- Duktape configuration header. **Not referenced by the build** (we use MuJS).
  Dead weight in the repo. Should be removed or the JS engine decision
  documented. See FIX-014.

## Cross-cutting issues

1. **No tests exist** despite README + CMake claiming a `ctest` flow. (FIX-000)
2. **No input bounds** on document size, node count, or nesting depth →
   DoS/OOM on hostile input. (FIX-015 / PERF-001)
3. **No memory arena** — every node/string is an individual malloc; teardown is
   a deep recursive free. High allocator pressure and fragmentation. (PERF-002)
4. **String-heavy hot paths** — tag classification, CSS matching, and style
   lookup all use repeated `strcasecmp`. Interning tags to an enum once would
   remove most of these. (FEAT-010 / PERF-005)
5. **Encoding** — input is assumed UTF-8; no charset detection or conversion
   from the HTTP header or `<meta charset>`. Latin-1 pages render mojibake.
   (MISS-006)
6. **No `file://` or local fixture loading** makes deterministic testing of the
   full pipeline hard. (FEAT-021)

## Severity tally (initial)

- P0 (crash/corruption): FIX-001, FIX-005 (NULL derefs), FIX-015 (OOM).
- P1 (wrong output): FIX-002, FIX-003, FIX-004, FIX-006, FIX-007, FIX-008,
  FIX-011, FIX-013, FEAT-002.
- P2 (missing capability): most FEAT/MISS items.
- P3 (polish): FIX-012, FIX-014, PERF-003.
