# FEAT-009 — Reload key and URL-bar caret/editing

- **Priority:** P2
- **Files:** `render.c`, `ui.c`

## Why

There is no reload (`r`/`F5`/`Ctrl+R`), and the URL bar has no caret, no
left/right cursor movement, no home/end, and edits only at the end. Editing a
long URL is painful and there is no way to refresh a page.

## Scope

1. **Reload:** `F5`, `Ctrl+R`, and `r` (when not focused) re-fetch the current
   URL (bypass any cache) and relayout, preserving scroll where sensible.
2. **Caret:** track a caret index into `search_query`; render a blinking caret.
3. **Editing:** Left/Right move the caret; Home/End jump; Backspace/Delete act
   at the caret; text input inserts at the caret; optional word-wise motion
   (Alt+Left/Right already used for history, so use Ctrl).
4. **Select-all/clear:** `Ctrl+A` selects, `Ctrl+U` clears (readline-ish).

## Micro-plan

1. Replace end-only mutation with caret-aware insert/delete helpers operating on
   `(buffer, len, caret)` — pure functions in `ui.c`.
2. Render the caret at the measured pixel offset of `search_query[0..caret]`.
3. Add reload handlers; reload uses the shared `navigate_to(current_url)` but
   without pushing a new history entry (replace, not push).

## Acceptance tests

- Unit: insert 'x' at caret 2 of "abcd" → "abxcd", caret 3.
- Unit: backspace at caret 0 is a no-op; at caret 3 of "abxcd" → "abcd".
- Unit: Home sets caret 0; End sets caret len.
- Smoke: reload re-runs the load path and does not grow history.

## Risk

Low. Text-edit logic is pure and unit tested; rendering the caret is cosmetic.
