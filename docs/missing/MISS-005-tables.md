# MISS-005 — Table layout

- **Priority:** P2
- **Files:** `layout.c`, new `table.{c,h}`

## Problem

`<table>` and friends are treated as generic blocks, so cells stack vertically
instead of forming a grid. Tabular data (specs, schedules, comparison tables —
common on reference sites) is unreadable.

## Scope (simple grid first)

1. Parse the table structure: `thead/tbody/tfoot` → rows (`tr`) → cells
   (`td/th`). Tolerate missing `tbody` (Gumbo inserts it).
2. **Column count** = max cells in any row (ignore `colspan`/`rowspan`
   initially; treat each cell as 1×1).
3. **Column widths:** two-pass auto layout —
   - Pass 1: measure each cell's preferred (max) and minimum (longest word)
     width.
   - Pass 2: distribute available table width across columns (proportional to
     preferred width, clamped to min), simple and robust.
4. **Row heights:** max cell height in the row after wrapping at the assigned
   column width.
5. Render cell borders/padding from UA defaults (FEAT-015) or `border`
   attribute.

## Phase 2 (P3)

- `colspan`/`rowspan`, `<caption>`, `border-collapse`, nested tables (cap
  nesting depth).

## Micro-plan

1. Build a logical grid model (rows × cols) from the DOM subtree under
   `<table>`.
2. Implement the two-pass width algorithm (pure function on measured widths —
   unit testable).
3. Lay out each cell as a block within its column width; compute row heights.
4. Emit boxes for cells (with borders) and table outline.

## Acceptance tests

- Unit: grid model from a 2×3 table (with `<th>` header row) has 3 columns,
  2 rows, cells in the right positions.
- Unit: width distribution — given column preferred widths [100,50,50] and table
  width 300, columns get widths summing to ≤300 with proportional split, each ≥
  its min.
- Golden `--dump`: a small table fixture lays out as a grid (cells side by side).

## Risk

High (table layout is genuinely complex). Mitigate by shipping the no-span grid
first, fully tested, and treating spans as a later, separately-tested phase.
