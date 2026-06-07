# MISS-009 — Accessibility affordances

- **Priority:** P3
- **Files:** `render.c`, `ui.c`, `style.c`

## Problem

No accessibility affordances: fixed text size (no zoom), no high-contrast theme,
no keyboard-only link traversal, no respect for reduced motion. A reading
browser should be exemplary here since reading is its whole purpose.

## Scope

1. **Text zoom:** `Ctrl++`/`Ctrl+-`/`Ctrl+0` scale a global font multiplier;
   relayout at the new scale. (Highest value; helps everyone.)
2. **High-contrast / dark theme:** part of FEAT-007 themes; ensure WCAG-ish
   contrast ratios for text vs background in provided themes.
3. **Keyboard link navigation:** Tab/Shift+Tab move a focus ring between link
   boxes; Enter activates. Works without a mouse.
4. **Respect prefers-reduced-motion:** disable caret blink / any animation when
   a flag is set.

## Note on compliance

Full WCAG conformance requires manual testing with assistive technologies and
expert review; we target the mechanical, high-impact items above and document
that formal conformance is not claimed.

## Micro-plan

1. Global `font_scale` applied in layout font-size resolution; zoom key
   handlers; relayout on change.
2. Themes with documented contrast ratios (compute and assert in a test).
3. Focus-ring state over link boxes; Tab cycling in document order; Enter
   navigates.

## Acceptance tests

- Unit: zoom multiplier changes resolved font sizes proportionally and clamps to
  a sane range.
- Unit: theme contrast ratio (luminance calc) for text/background meets a
  threshold (e.g. ≥ 4.5:1) for the light and dark themes.
- Unit: Tab focus order over a set of link boxes follows document order and
  wraps.

## Risk

Low. Each item is self-contained; zoom and focus order are pure logic and unit
tested.
