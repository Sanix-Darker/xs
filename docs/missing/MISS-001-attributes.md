# MISS-001 — Generic attribute capture

- **Priority:** P1 (foundation for FEAT-004, FEAT-013, FEAT-014, MISS-002/004)
- **Files:** `parser.{c,h}`, `dom.{c,h}`

## Problem

The parser captures only `href` on `<a>`. Everything else — `class`, `id`,
`src`, `alt`, `title`, `style`, `rel`, `type`, `width`, `height`, form
attributes — is discarded. This single gap blocks CSS class/id selectors, inline
styles, external stylesheets, images, and forms.

## Design

```c
typedef struct { const char* name; const char* value; } Attr; // interned/arena
struct DOMNode {
    ...
    Attr* attrs;
    int   n_attrs;
};
const char* dom_attr(const DOMNode* n, const char* name);   // NULL if absent
int         dom_has_class(const DOMNode* n, const char* cls);
const char* dom_id(const DOMNode* n);
```

- During `parse_gumbo_node`, iterate `element->attributes` (a `GumboVector` of
  `GumboAttribute*`). Gumbo attribute `name` and `value` are NUL-terminated
  C strings owned by Gumbo; copy them into the arena (length-bounded to be
  safe).
- Keep the existing `href` convenience pointer (or derive it via `dom_attr`).
- `dom_attr` is case-insensitive on the name; class/id get fast helpers.

## Memory

Attributes are arena-allocated (MISS-007) so capture adds no per-node free
burden. Most elements have 0–3 attributes; store as a small contiguous array.

## Micro-plan

1. Add `attrs`/`n_attrs` to `DOMNode`; capture in the parser.
2. Implement `dom_attr`, `dom_has_class`, `dom_id`.
3. Migrate `<a>` href to use the generic path (keep the cached pointer for hot
   link rendering).

## Acceptance tests

- Unit: `<a href="x" class="c1 c2" id="i">` → `dom_attr(...,"href")=="x"`,
  `dom_has_class(...,"c2")` true, `dom_id(...)=="i"`.
- Unit: `dom_attr` case-insensitive; absent attr → NULL.
- Unit: an element with many attributes captures all of them.

## Risk

Low–medium. Struct change + parser addition. Do it early; many features depend
on it.
