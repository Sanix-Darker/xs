# FEAT-005 — Typed cascade with specificity and inheritance

- **Priority:** P2
- **Files:** new `style.{c,h}`, `select.c`, `css.c`

## Why

Today "cascade" is "last matching rule wins per property", with values stored as
raw strings and re-parsed at layout time. There is no specificity, no source
order tiebreak, no inheritance (so `color`/`font-size` set on `<body>` do not
reach descendants), and no UA defaults. This produces incorrect styling whenever
two rules touch the same element.

## Design

Typed computed style:

```c
typedef struct {
    Color   color;          // resolved RGBA
    Color   background;      // resolved RGBA (or transparent)
    float   font_size_px;    // resolved
    uint8_t font_weight;     // 0 normal / 1 bold
    uint8_t font_style;      // 0 normal / 1 italic
    uint8_t text_align;      // 0/1/2/3
    Length  width, height;
    Edge    margin, padding; // top/right/bottom/left (FEAT-006)
    Border  border;
    uint8_t display;         // none/inline/block/...
    /* inherited flags computed during resolve */
} ComputedStyle;
```

Cascade algorithm:

1. Gather all `(selector, declaration)` pairs that match a node, each with a
   computed **specificity** triple `(ids, classes, types)` and source order.
2. Sort by origin (UA < author), then specificity, then source order.
3. Apply in order; later wins. `!important` (P3) bumps priority.
4. **Inheritance:** inherited properties (`color`, `font-*`, `text-align`,
   `line-height`, `visibility`) fall back to the parent's computed value when
   not specified. Resolve top-down so parents are computed first.
5. **UA defaults** seed the initial values (FEAT-015).

## Micro-plan

1. Define `Color`, `Length` (FIX-007), `Edge`, `Border`, `ComputedStyle` in
   `style.h`.
2. `Color parse_color(const char*)`: `#rgb`, `#rrggbb`, `rgb()`/`rgba()`, and a
   small named-color table (black, white, red, ... ~16). Unknown → inherit/none.
3. `specificity(ComplexSel)` → packed int.
4. `style_resolve(Document)` walks the tree top-down, computing
   `ComputedStyle` per element from matched rules + inheritance + UA defaults.
5. Store the resolved `ComputedStyle` on the node (arena-allocated); layout reads
   typed values, never re-parses strings.

## Acceptance tests

- Unit: `parse_color("#fff")==parse_color("#ffffff")==white`; `rgb(255,0,0)` ==
  red; named `red` == red; bad input → sentinel.
- Unit: specificity `#id` > `.cls` > `tag`; equal specificity → later rule wins.
- Unit: `body{color:red}` makes a nested `<p>` text red via inheritance.
- Unit: a more specific rule overrides a less specific one regardless of order.

## Risk

Medium–high (the heart of CSS correctness). Build incrementally: colors →
specificity sort → inheritance → box model. Each step independently tested.
