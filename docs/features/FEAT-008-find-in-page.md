# FEAT-008 — Find-in-page (`Ctrl+F`)

- **Priority:** P3
- **Files:** `render.c`, `ui.c`

## Why

A reading browser benefits hugely from in-page search. It is self-contained and
exercises the text-box index in a useful way.

## Design

1. `Ctrl+F` opens a find field (reuse the search-bar input mechanics with a mode
   flag: `MODE_URL` vs `MODE_FIND`).
2. On query change, scan all text `LayoutBox`es for case-insensitive substring
   matches; collect matching box indices.
3. Highlight matches (draw a translucent rect behind matched boxes). Track a
   "current match"; `Enter`/`F3` cycles; auto-scroll so the current match is in
   view.
4. `Esc` closes find and clears highlights.

Because text is stored per word/run in boxes, a match that spans box boundaries
(across wrapped words) is matched at the word level first; cross-box phrase
matching is P3-of-P3.

## Micro-plan

1. Add an input mode enum and route `SDL_TEXTINPUT` to the active field.
2. `find_collect(layout, query)` → array of box indices (pure, testable).
3. `find_scroll_to(index)` sets `scroll_offset` to center the match.
4. Render highlights in `render_content` when find mode is active.

## Acceptance tests

- Unit: `find_collect` over a known layout returns the correct box indices for a
  query, case-insensitively; empty query → no matches.
- Unit: `find_scroll_to` computes an offset that places the box within
  `[SEARCH_BAR_HEIGHT, window_h]`.

## Risk

Low. Self-contained; pure match/scroll logic is unit tested.
