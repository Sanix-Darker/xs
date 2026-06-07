# MISS-004 — Forms & basic input

- **Priority:** P3
- **Files:** `layout.c`, `render.c`, `ui.c`, `network.c`

## Problem

No form support: `<form>`, `<input>`, `<button>`, `<textarea>`, `<select>` are
treated as generic blocks/inline with no interaction. Search boxes and simple
GET forms (the most common interactive element on reading sites) do not work.

## Scope (minimal)

1. **Text inputs** (`<input type=text|search|email|url>`): render a focusable
   field; type into it; caret; reuse the URL-bar text-edit logic (FEAT-009).
2. **Buttons / submit** (`<button>`, `<input type=submit>`): clickable.
3. **GET form submission:** on submit, gather named fields, build a query string,
   resolve `action` against the base URL, and navigate. (POST is P3-of-P3.)
4. `<textarea>` multi-line text (basic). `<select>`/checkbox/radio later.

## Out of scope (initially)

- POST, file upload, validation, JS-driven form events, autocomplete.

## Micro-plan

1. Capture form-related attributes (`name`, `value`, `type`, `action`,
   `method`, `placeholder`) via MISS-001.
2. Layout: input/button boxes with intrinsic sizes; track focusable elements.
3. Interaction: focus management (Tab to move between fields), text editing in
   the focused field, click to focus/submit.
4. Submit: build `action?name1=val1&name2=val2` (URL-encoded), `navigate_to`.

## Acceptance tests

- Unit: query-string builder URL-encodes names/values and joins with `&`.
- Unit: `action` resolves relative to base correctly.
- Unit (interaction logic): focus cycling visits fields in document order.
- Integration: a fixture GET form submits to the expected URL.

## Risk

Medium. Interaction/focus state adds complexity. Keep to GET + text/button first
and lock the query-string builder with unit tests.
