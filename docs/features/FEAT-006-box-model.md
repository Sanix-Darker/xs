# FEAT-006 — Real margin/padding/border box model

- **Priority:** P2
- **Files:** `layout.c`, `style.{c,h}`

## Why

Spacing is currently hardcoded constants (`PARAGRAPH_SPACING`, `BLOCK_SPACING`,
`LIST_INDENT`, ...). CSS `margin`/`padding`/`border-width` are ignored entirely.
Pages that rely on margins/padding for structure collapse into a wall of text.

## Scope

Implement the standard block box model for block-level boxes:

- `margin` (top/right/bottom/left), `padding`, `border-width`.
- Shorthand expansion: `margin: a`, `margin: a b`, `margin: a b c`,
  `margin: a b c d`; same for `padding`, `border-width`.
- Content width = containing width − margins − borders − paddings (for `width:
  auto`); explicit `width` honored within that.
- Vertical **margin collapsing** between adjacent block siblings and
  parent/first-child (simplified but correct for the common cases).
- Borders drawn by render using the computed border widths/colors (replaces the
  ad-hoc wireframe).

Inline boxes keep simple horizontal advance; inline padding/margin is P3.

## Micro-plan

1. `Edge`/`Border` types in `style.h`; shorthand expanders in `style.c`.
2. Layout computes, per block: outer box (x,y at margin edge), border box,
   padding box, content box. Children flow in the content box.
3. Advance `cur_y` by `margin-top` (after collapsing), lay out content, add
   `padding`, `border`, then `margin-bottom` (collapsed with next sibling's
   margin-top).
4. Replace hardcoded spacing with UA-stylesheet margins (FEAT-015) so headings/
   paragraphs/lists get their spacing from default CSS, not magic numbers.
5. Render: draw border rectangles from computed border, background fill from
   computed background over the border/padding box.

## Acceptance tests

- Unit: `margin: 10px 20px` expands to top/bottom=10, left/right=20.
- Unit: a block with `padding:10px;border:2px;width:100px` inside a 300px
  container has content box width 100 and border box width 124, positioned at
  x = container_x + margin_left.
- Unit: two stacked `<p>`s with `margin:10px 0` collapse to 10px gap (not 20).
- Golden `--dump` locks coordinates for a small fixture.

## Risk

High — box model + margin collapsing is intricate. Stage it: edges/shorthand →
non-collapsing block model → margin collapsing. Gate behind tests at each stage.
Keep the old constant-based path available behind a flag until parity is proven.
