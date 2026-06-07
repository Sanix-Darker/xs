# MISS-003 — JS host bindings (console / minimal document)

- **Priority:** P2
- **Files:** `javascript.{c,h}`, new `dom_js.{c,h}`

## Problem

MuJS runs raw script with **no host objects**: no `console`, `window`,
`document`, `setTimeout`, navigator, location. So scripts cannot log, cannot read
or change the page, and (because of FIX-002) often do not even parse. JS is
effectively a no-op with invisible failures.

## Scope (minimal, read-mostly — aligned with non-goals)

We are explicitly **not** building WHATWG DOM. We expose a small, useful host:

1. `console.log/info/warn/error` → print to stderr/status (immediate value;
   makes scripts debuggable).
2. `window` as the global object; `navigator.userAgent`; `location.href`
   (read), `location.assign(url)` (navigate — wired to the browser navigate).
3. **Read-only `document` queries:**
   `document.title` (get),
   `document.getElementById(id)` → an opaque wrapper exposing `.textContent`
   (get), `.getAttribute(name)`.
   `document.querySelector` (subset, reusing FEAT-004 matcher) — P3.
4. `document.write` is **not** supported (security/complexity); calls are
   ignored with a console warning.

Mutations that affect layout (`element.textContent = ...`, `style.*`) are
**out of scope initially**; if added later, they must trigger a relayout. Start
read-only to keep correctness tractable.

## Dependencies

- FIX-002 (intact script text), MISS-001 (attributes), FEAT-010 (parent/tag for
  queries).

## Micro-plan

1. Register `console` with native functions via MuJS C API (`js_newcfunction`,
   `js_setglobal`).
2. Register `window`/`navigator`/`location` with getters and a native
   `assign`/href setter that calls back into the browser's navigate.
3. Wrap `DOMNode*` as a MuJS userdata with a small prototype
   (`textContent` getter, `getAttribute`).
4. Implement `getElementById` by walking the (interned-id-indexed) DOM.
5. Surface real MuJS error messages (file/line) instead of the generic string.

## Acceptance tests

- Unit (headless, link MuJS): evaluating
  `console.log('hi'); var t = document.title;` runs without error and
  `console.log` is observed (capture via a test sink).
- Unit: `document.getElementById('x').getAttribute('data-y')` returns the
  expected value for a fixture DOM.
- Unit: a syntax error reports a useful message (not just "Script error").

## Risk

Medium. MuJS C-API binding is fiddly but well-documented. Keeping it read-only
avoids the relayout/consistency rabbit hole.
