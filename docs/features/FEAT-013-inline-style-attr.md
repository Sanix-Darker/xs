# FEAT-013 — Inline `style=""` attribute support

- **Priority:** P2
- **Files:** `parser.c` (attribute capture), `css.c`/`style.c`

## Why

Inline `style="..."` is extremely common and has the highest cascade priority
after `!important`. Without it, many pages lose critical layout/color hints.
Requires generic attribute capture (also needed by FEAT-004 selectors and
MISS-002 images).

## Design

1. Parser captures all attributes per element into a small list:
   ```c
   typedef struct { const char* name; const char* value; } Attr;  // interned
   DOMNode { ...; Attr* attrs; int n_attrs; }
   const char* dom_attr(const DOMNode*, const char* name); // case-insensitive
   ```
2. During cascade, if a node has `style`, parse it as a declaration block
   (reuse the CSS declaration parser on the attribute value) and apply it with
   the highest non-important specificity (overrides author rules).

## Micro-plan

1. Add attribute capture in `parse_gumbo_node` using
   `gumbo_get_attribute`/iterating `element->attributes` (names are
   NUL-terminated in Gumbo; values too — verify and copy with length to be
   safe).
2. `dom_attr()` accessor (case-insensitive name compare; interned fast path).
3. In `style_resolve`, after author rules, parse and apply `style` attribute
   declarations.

## Acceptance tests

- Unit: parse `<p style="color:red; font-size:20px">` → `dom_attr(node,"style")`
  returns the string; cascade yields red color and 20px font.
- Unit: an inline `style` color overrides an author `p{color:blue}` rule.
- Unit: `dom_attr` is case-insensitive (`STYLE` vs `style`).

## Risk

Low–medium. Reuses the declaration parser. Attribute capture is a small parser
addition that several features depend on — do it early.
