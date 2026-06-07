# FEAT-012 — Visited-link state + hover cursor

- **Priority:** P3
- **Files:** `render.c`, `ui.c`

## Why

All links render identically and there is no hover affordance. Visited-link
coloring and a hand cursor on hover are small touches that make navigation feel
real.

## Design

1. **Visited set:** a bounded hash set of visited URLs (normalized). On
   navigate, add the resolved URL. When rendering a link box, if its resolved
   target is in the set, use the "visited" color.
2. **Hover:** on `SDL_MOUSEMOTION`, hit-test link boxes; if over a link, set the
   system cursor to `SDL_SYSTEM_CURSOR_HAND` and show the target in the status
   line (FEAT-007); else arrow cursor.
3. Bound the visited set (e.g. last 4096 URLs, LRU) so long sessions do not grow
   memory.

## Micro-plan

1. `visited_add(url)` / `visited_contains(url)` over a small open-addressing
   hash with LRU eviction (pure, testable).
2. Resolve each link box's target once (cache the resolved string) to avoid
   re-resolving on every frame during hover.
3. Cursor management via `SDL_CreateSystemCursor`/`SDL_SetCursor`.

## Acceptance tests

- Unit: `visited_add` then `visited_contains` true; a non-added URL false;
  eviction keeps the set bounded.
- Unit: hit-test returns the correct link box for a point inside it, none
  outside.

## Risk

Low. Visited coloring and cursor are cosmetic; the hash set is unit tested.
