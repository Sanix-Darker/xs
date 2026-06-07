# FEAT-007 — Scrollbar, title, status, themes

- **Priority:** P2
- **Files:** `render.c`, new `ui.{c,h}`

## Why

The window has a search bar but no scroll position indicator, no page title, no
loading/status feedback, and the always-on wireframe borders add noise. These
are low-risk quality-of-life wins that make the browser usable.

## Scope

1. **Scrollbar:** a thin right-edge indicator showing
   `view_h/content_height` thumb at `scroll_offset` ratio. Draggable (P3); at
   minimum visual.
2. **Title:** show the page `<title>` in the SDL window title bar
   (`SDL_SetWindowTitle`) and/or in the chrome.
3. **Status line:** transient text for "Loading…", final URL, link target on
   hover, and error messages. Bottom strip or overlaid in the search bar area.
4. **Theme toggle:** Kindle warm (current), light, dark, and serif/sans choice
   (repo ships DejaVuSerif). Cycle with a key (e.g. `t`). Colors come from a
   `Theme` struct, not scattered `#define`s.
5. **Wireframe toggle:** move structural borders behind the box model
   (FEAT-006) and a debug toggle (`d`) rather than always-on.

## Micro-plan

1. Extract colors into a `Theme` struct; replace `BG_R/G/B` and inline color
   literals with theme fields.
2. Add `render_scrollbar()`, `render_status()`; call from the draw path.
3. `SDL_SetWindowTitle(win, doc->title)` on navigate.
4. Track hover: on `SDL_MOUSEMOTION`, hit-test links, set status to the resolved
   target and switch the cursor to a hand (FEAT-012).
5. Key handlers for theme/debug toggles (only when search not focused).

## Acceptance tests

- Logic unit: scrollbar thumb geometry — given content_height, view_h,
  scroll_offset, compute thumb y/height; assert clamps at top/bottom.
- Logic unit: theme cycling returns the expected sequence and never NULL.
- Smoke: window title equals the fixture's `<title>` after load.

## Risk

Low. Mostly additive rendering. Keep geometry math in `ui.c` so it is unit
testable without SDL.
