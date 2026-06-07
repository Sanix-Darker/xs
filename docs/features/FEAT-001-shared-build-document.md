# FEAT-001 — Single `build_document` pipeline

- **Priority:** P1 (foundational; unblocks FIX-002, FIX-009, FIX-010)
- **Files:** new `document.{c,h}`, `main.c`, `render.c`

## Why

The load pipeline is duplicated in `main.c` and `render.c reload_page`, and the
two copies already disagree on ordering (CSS/script vs text-split). Duplication
is how FIX-002 crept in. There must be exactly one function that turns bytes (or
a URL) into a ready-to-lay-out `Document`.

## Design

```c
typedef struct {
    Arena*    arena;     // owns all node/string memory (PERF-002)
    DOMNode*  root;
    char*     title;     // from <title> (arena or strdup)
    char      base_url[2048];
    /* future: stylesheet list, script results, charset */
} Document;

Document* document_from_html(const char* html, size_t len,
                             const char* base_url, const char* charset);
Document* document_from_fetch(const FetchResult* fr);   // wraps the above
void      document_free(Document*);
```

`document_from_html` performs, in the correct order:

1. Parse HTML → arena DOM (FIX-001 safe tags, FIX-015 caps).
2. Extract `<title>`.
3. Extract + parse + cascade CSS (UA sheet + `<style>` + inline + external).
4. Run scripts (after DOM/CSS, with intact text — FIX-002).
5. Return the Document. **No per-word splitting** (FEAT-011 moves it to layout).

## Micro-plan

1. Create `document.{c,h}`; move pipeline logic out of `main.c`/`render.c`.
2. `main.c`: `fetch → document_from_fetch → render_layout(doc, url)`.
3. `render.c reload_page`: same call; remove its private copy.
4. Delete `split_text_nodes` from the pipeline (kept only if FEAT-011 is staged
   later; otherwise removed).

## Acceptance tests

- Unit: `document_from_html("<title>Hi</title><p>x</p>", ...)` yields
  `title=="Hi"` and a root with a `p`.
- Both entry points produce byte-identical layout for the same input (golden
  test via `--dump`).

## Risk

Medium — central refactor. Do it early, behind the growing test suite.
