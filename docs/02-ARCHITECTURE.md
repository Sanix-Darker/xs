# 02 — Architecture: Current and Target

## Current architecture

```
                 ┌─────────────┐
   URL ────────▶ │  network.c  │  libcurl blocking fetch → char* html
                 └──────┬──────┘
                        ▼
                 ┌─────────────┐
                 │  parser.c   │  Gumbo → DOMNode tree (malloc per node)
                 │             │  split_text_nodes (1 node per word)
                 └──────┬──────┘
                        ▼
                 ┌─────────────┐
                 │   css.c     │  flat stylesheet, tag-name match only
                 └──────┬──────┘
                        ▼
                 ┌─────────────┐
                 │javascript.c │  MuJS eval, no DOM bindings
                 └──────┬──────┘
                        ▼
                 ┌─────────────┐
                 │  layout.c   │  recursive single pass → LayoutBox[]
                 └──────┬──────┘
                        ▼
                 ┌─────────────┐
                 │  render.c   │  SDL2 loop, font/texture cache, history
                 └─────────────┘
```

### Ownership today (fragile)
- `parse_html` allocates the DOM (many small mallocs).
- `render_layout` is handed the DOM and is responsible for freeing it.
- `Layout` optionally owns the DOM (`free_layout` frees `lay->dom`).
- On resize/navigate, code manually detaches/reattaches `dom` to avoid double
  free. Easy to get wrong.

## Target architecture

Same pipeline shape (it is sound for a reading browser), but with a hardened
memory model, typed styles, interned tags, and clear ownership.

```
   URL
    │
    ▼
 ┌───────────────┐   FetchResult { body, len, status, content_type, charset }
 │   network.c   │   - size cap, status, content-type, geometric buffer growth
 └──────┬────────┘   - reusable curl handle
        ▼
 ┌───────────────┐   Document { Arena, DOMNode* root, StringPool }
 │   parser.c    │   - arena-allocated nodes & strings
 │   + dom.c     │   - interned tag ids, parent pointers, attribute list
 │               │   - whitespace-correct text (NO per-word shredding)
 └──────┬────────┘
        ▼
 ┌───────────────┐   CSSStyleSheet (typed declarations)
 │    css.c      │   - comment-aware tokenizer
 │   + select.c  │   - tag/class/id/grouping/universal selectors + specificity
 └──────┬────────┘   - cascade into typed ComputedStyle (box model, color)
        ▼
 ┌───────────────┐
 │ javascript.c  │   - console.log, minimal document.* read API (optional)
 │   + dom_js.c  │
 └──────┬────────┘
        ▼
 ┌───────────────┐   Layout { Arena, LayoutBox[] }
 │   layout.c    │   - typed box model, text-align, real wrapping by glyph runs
 │   + text.c    │   - measure at true font size
 └──────┬────────┘
        ▼
 ┌───────────────┐
 │   render.c    │   - content-keyed texture cache, scrollbar, title, status
 │   + ui.c      │   - italic synthesis, image blits
 └───────────────┘
```

### Memory model (the core change)

Introduce a bump/arena allocator (`arena.c`):

- One `Arena` per `Document`. All `DOMNode`, attribute, and string allocations
  for a page come from the arena.
- Teardown is **O(1)**: free the arena's blocks; no deep recursive walk, no
  per-node `free`. This slashes allocator pressure and eliminates whole classes
  of use-after-free/double-free bugs.
- Layout boxes come from a second arena (or a single growable array, already
  the case) tied to the layout, not the DOM, so relayout-on-resize reuses the
  DOM arena untouched.

### String interning

`StringPool` maps a byte span → stable `const char*` (arena-owned) + an integer
id. Tag names are interned to a small enum (`TagId`) at parse time:

- Layout/CSS classification becomes an integer compare or a table index, not
  `strcasecmp`.
- The texture cache can key on (interned text id, size, bold) safely across the
  page's lifetime.

### Ownership, finalized

- `Document` owns its `Arena` (DOM + strings). Freed once, explicitly.
- `Layout` owns only its box array; it holds a **borrowed** pointer to the
  `Document`. `free_layout` never frees the DOM.
- `render.c` owns the current `Document` and current `Layout` separately:
  - resize → free `Layout`, rebuild from the same `Document`.
  - navigate → build new `Document` + `Layout`, then free the old `Layout`
    then the old `Document`.

This removes the manual detach/reattach dance entirely.

## Module map (target)

| File | Responsibility |
|------|----------------|
| `arena.{c,h}` | bump allocator, block list, reset/free |
| `strpool.{c,h}` | string interning + tag enum |
| `network.{c,h}` | fetch with caps/status/content-type, handle reuse |
| `parser.{c,h}` | Gumbo → arena DOM, attributes, whitespace-correct text |
| `dom.{c,h}` | DOMNode helpers, attribute access, tree queries |
| `css.{c,h}` | tokenizer + parser (comment-aware) |
| `select.{c,h}` | selector matching + specificity + cascade |
| `style.{c,h}` | typed ComputedStyle, color/length parsing, inheritance |
| `layout.{c,h}` | box model layout, text-align, wrapping |
| `text.{c,h}` | text measurement, word/break iteration, whitespace collapse |
| `javascript.{c,h}` | MuJS host + minimal console/document bindings |
| `render.{c,h}` | SDL2 loop, caches, scrollbar, UI chrome |
| `ui.{c,h}` | search bar, scrollbar, status, hit-testing |
| `main.c` | wiring + CLI (url, `file://`, `--dump`, flags) |

We will not create all of these at once. The roadmap sequences them so the
browser stays buildable and testable at every step.

## Threading model

Single-threaded, event-driven. Network fetch is synchronous for now (a blocking
`curl_easy_perform`). A later enhancement may move fetch to a worker thread to
keep the UI responsive (FEAT-022), but that is explicitly post-correctness.

## Error handling philosophy

Every fallible function returns a status or NULL and never aborts. The top of
the pipeline converts any failure into an on-screen error document. Asserts are
used only for true invariants (never on input-derived data).
