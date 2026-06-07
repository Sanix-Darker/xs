# FEAT-002 — Apply `text-align` in layout/render

- **Priority:** P1 (already parsed/stored but ignored — README admits it)
- **Files:** `layout.c`, `render.c`

## Why

`ComputedStyle.text_align` is parsed and stored but never affects output.
Centered headings, right-aligned bylines, and centered figure captions all
render left, which looks broken on many pages.

## Design

Text alignment is a property of the **line box**. The current layout emits one
`LayoutBox` per word as it flows; to align, we must know each line's total used
width and the available width, then shift that line's boxes.

Approach (works with the flat box array):

1. During block layout, record the index range of boxes that belong to each
   line and the line's `used_width` and `line_start_x`/`avail_w`.
2. At line break (and block end), if the block's `text-align` is `center` or
   `right`, compute `delta = avail_w - used_width` (center: `delta/2`), and add
   `delta` to the `x` of every box on that completed line.
3. `justify` (optional, P3): distribute `delta` across inter-word gaps.

Alignment is inherited from the nearest block ancestor that sets it; resolve via
the cascade (FEAT-005) or, interim, read `node->style->text_align` on the block
and thread it through `LayoutContext`.

## Micro-plan

1. Add `int align; /* 0 left,1 center,2 right,3 justify */` to `LayoutContext`,
   set from the block's computed `text-align`.
2. Track `line_first_box_index` and `line_used_width` in the block layout loop.
3. On each line flush, post-adjust the `x` of `[line_first_box_index, count)`.
4. No render change needed (render draws boxes at their `x`).

## Acceptance tests

- Unit: lay out `<div style="text-align:center"><p>word word</p></div>` in a
  known width; assert the line's boxes are shifted so the line is centered
  (left edge ≈ (avail - used)/2 within rounding).
- Unit: `text-align:right` pushes the line flush to the right margin.
- Golden `--dump` includes x-coordinates to lock alignment.

## Risk

Medium — requires per-line bookkeeping in the single-pass layout. Keep it to
post-adjustment of already-placed boxes to avoid a second pass.
