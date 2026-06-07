# FEAT-004 — Class/id/group/universal selectors

- **Priority:** P2
- **Files:** `css.c`, new `select.{c,h}`, `parser.c` (attribute capture)

## Why

Selector matching is tag-name-only. Real stylesheets target `.class`, `#id`,
grouped selectors (`h1, h2`), the universal `*`, and simple descendant
combinators (`nav a`). Without these, almost no real CSS applies, so pages fall
back to UA defaults and look unstyled.

## Scope (pragmatic subset)

Support, in order of value:

1. Type: `p`
2. Universal: `*`
3. Class: `.note`, compound `p.note`
4. Id: `#main`
5. Grouping: `a, b, c` (split into N simple selectors sharing declarations)
6. Descendant combinator: `nav a` (ancestor match via parent pointers)
7. Child combinator `>` (P3), attribute selectors `[type=text]` (P3),
   pseudo-classes (mostly ignored; `:hover`/`:visited` later via FEAT-012).

## Dependencies

- Parser must capture `class` and `id` attributes (and a general attribute list)
  — see FEAT-013/MISS-001. Class is space-separated; store the raw string and
  match token-wise.
- Parent pointers on `DOMNode` for descendant matching — see FEAT-010.

## Design

```c
typedef enum { SEL_TYPE, SEL_UNIVERSAL, SEL_CLASS, SEL_ID } SimpleKind;
typedef struct { SimpleKind kind; const char* name; } SimpleSel;  // one piece
typedef struct {                  // a compound + combinator chain
    SimpleSel parts[ ... ];       // e.g. p.note#x = 3 parts (same element)
    int n_parts;
    struct CompoundSel* ancestor; // for "nav a", ancestor must match somewhere
} ComplexSel;
```

`parse_selector_list(const char* s)` splits on top-level commas into N
`ComplexSel`, each pointing at the rule's declarations. `selector_matches(sel,
node)` checks the rightmost compound against the node, then walks parents for
ancestor compounds.

## Micro-plan

1. Add attribute capture (`class`, `id`, generic list) in `parser.c`.
2. Add parent pointers (FEAT-010).
3. Implement the selector parser + matcher in `select.c`.
4. Replace `matches_selector` in `css.c` with `selector_matches`.

## Acceptance tests

- Unit: `.note` matches `<p class="a note b">`, not `<p class="notes">`.
- Unit: `#main` matches `id="main"` only.
- Unit: `h1, h2` applies to both.
- Unit: `nav a` matches an `<a>` inside `<nav>`, not a top-level `<a>`.
- Unit: `*` matches any element.

## Risk

Medium. Combinators + specificity interact with FEAT-005; keep the matcher pure
and unit-tested in isolation.
