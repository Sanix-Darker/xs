# FIX-009 — Fragile DOM ownership and free paths

- **Severity:** P1 (use-after-free / double-free risk)
- **Files:** `render.c`, `layout.c`

## Problem

DOM ownership is ambiguous and manually juggled:

- `free_layout` frees `lay->dom` (layout owns DOM).
- On resize, `render.c` does:
  ```c
  DOMNode *dom_ref = currentLayout->dom;
  currentLayout->dom = NULL;        // detach so free_layout won't free DOM
  free_layout(currentLayout);
  currentLayout = layout_dom(dom_ref, ...);
  ```
  i.e., it relies on nulling `dom` to avoid freeing the still-needed tree.
- `navigate_to` and back/forward call `free_layout(currentLayout)` which **does**
  free the DOM, then build a fresh layout from a fresh DOM.
- `render_layout` is also documented to "take ownership of dom" and frees
  `currentLayout` at the end.

This works only because of the careful detach dance. Any new code path that
forgets to detach will double-free or use-after-free. It also means relayout
re-builds nothing but boxes yet conceptually "frees the layout that owns the
DOM".

## Evidence

`render.c` resize handler (detach), `navigate_to`, Alt+Left/Right handlers, and
`free_layout` in `layout.c`.

## Micro-plan

Adopt the ownership model from `02-ARCHITECTURE.md`:

1. Introduce a `Document` that owns the DOM (eventually via an arena). `Layout`
   holds a **borrowed** `Document*` and owns only its box array.
2. `free_layout` frees boxes only — **never** the DOM.
3. `render.c` keeps `Document *currentDoc` and `Layout *currentLayout`
   separately:
   - resize: `free_layout(currentLayout); currentLayout = layout_dom(currentDoc, ...)`.
   - navigate/history: build `newDoc`+`newLayout`; then
     `free_layout(currentLayout); free_document(currentDoc);` and swap in the
     new ones.
4. Remove all `dom = NULL` detach hacks.
5. `render_layout` signature changes to take a `Document*` (or builds the first
   one internally); `main.c` updated accordingly.

## Acceptance tests

- Leak test (driver harness, headless): construct a Document, lay it out, free
  layout, re-lay out 100×, free document once → ASan/LeakSan report 0 leaks and
  no double-free.
- Navigate simulation: build doc A, doc B, swap and free A → no UAF (ASan).

## Risk

Medium — touches render lifetime. Mitigated by the headless leak harness that
exercises the swap/relayout/free sequence without a window.
