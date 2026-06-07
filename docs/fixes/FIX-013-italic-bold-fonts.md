# FIX-013 — Italic never synthesized; bold font missing

- **Severity:** P1 (inline emphasis invisible)
- **Files:** `render.c`, repo fonts

## Problem

1. The repo ships `DejaVuSans.ttf` and `DejaVuSerif.ttf` but **not**
   `DejaVuSans-Bold.ttf`. `get_font(size, bold=1)` first tries the bold file,
   fails, then falls back to the **regular** file — so bold text is not actually
   bold unless a system bold font is found.
2. `is_italic` is tracked through layout (`LayoutHints.is_italic`) and even
   stored, but **render.c never uses it**. There is no italic font and no
   `TTF_SetFontStyle(..., TTF_STYLE_ITALIC)`. So `<em>`/`<i>` are
   indistinguishable from body text.

## Evidence

- Only two TTFs present (`ls *.ttf`).
- `render.c get_font` ignores italic entirely; `font_cache` key is `(size,
  bold)` with no italic dimension.
- `LayoutHints.is_italic` is set in layout but never read in `render_content`.

## Micro-plan

1. Extend the font cache key to `(size, bold, italic)`.
2. Use `TTF_SetFontStyle` to synthesize bold and/or italic from the base face
   when a dedicated bold/italic TTF is not available:
   `TTF_STYLE_BOLD | TTF_STYLE_ITALIC` as needed. (Synthesized bold/italic is
   acceptable for a reading browser and removes the missing-font dependency.)
   Note: applying a style flag mutates the `TTF_Font`; since each cache entry is
   a distinct `TTF_Font*`, set the style once at open time per entry.
3. Prefer a real bold/italic face if present; else synthesize. Document the
   font search order.
4. Pass `italic` from `LayoutHints` into `get_font` in the text render path and
   into measurement (FIX-008).
5. Optionally bundle a serif option for the Kindle aesthetic (the repo already
   ships DejaVuSerif) — behind a flag (FEAT-007).

## Acceptance tests

- Unit (font key): cache distinguishes (16,bold=1,italic=0) from
  (16,bold=0,italic=1) — three distinct entries for B/I/BI.
- Visual smoke (`--dump` cannot show style, so) a render smoke test asserts
  `get_font` returns non-NULL for all four style combinations and that the
  styled font's measured width for an italic string differs appropriately.

## Risk

Low. `TTF_SetFontStyle` is standard. Synthesized italic shear is cosmetic.
