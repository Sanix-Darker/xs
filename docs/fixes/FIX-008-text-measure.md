# FIX-008 — Text measured at wrong font size

- **Severity:** P1 (mis-wrapping, especially headings)
- **File:** `layout.c` (`measure_text_width`)

## Problem

```c
static int measure_text_width(void *font, const char *text, int target_size) {
    if (font && text && *text) {
        int w = 0;
        TTF_SizeUTF8((TTF_Font*)font, text, &w, NULL);
        return w * target_size / FONT_BODY;   // scale a 16pt measurement
    }
    return text ? (int)strlen(text) * 7 * target_size / FONT_BODY : 0;
}
```

Layout is always handed a **single** 16pt font and scales its measurement
linearly by `target_size/16`. Glyph widths are **not** linear in point size
(hinting, rounding, kerning differ), so headings (28/24/20pt) and small text
(13/14pt) get wrong widths → premature or late wrapping, overlapping text, and
boxes that do not match what render.c later draws (render.c uses correctly-sized
fonts, so layout and paint disagree).

## Evidence

`layout.c measure_text_width` and the single `get_font(16,0)` passed as the
layout font in `render.c`.

## Micro-plan

1. Give layout access to a **font provider** instead of one font: a callback
   `int (*measure)(void* ctx, const char* text, int px, int bold, int italic)`
   or pass a small `FontSet` handle that can fetch a sized/bold font from the
   render font cache.
2. `text.c measure_text_width(provider, text, size, bold, italic)` calls
   `TTF_SizeUTF8` on the **actual** font for that size/style.
3. Keep the `font==NULL` headless fallback (approximate by
   `strlen * k * size/16`) for unit tests, but make it explicitly the test path.
4. Cache measurements (size,bold,italic,text-ptr) to avoid repeated
   `TTF_SizeUTF8` during relayout — ties into PERF-004.

## Acceptance tests

- With a real font, the measured width of a string at 28pt equals
  `TTF_SizeUTF8` at 28pt (not 16pt scaled). Assert they differ from the scaled
  value for at least one heading string (proves the bug is fixed).
- Headless: `measure(NULL, "hello", 16,...)` returns a positive deterministic
  value so layout tests are stable.

## Risk

Medium — changes the layout/render font plumbing. The provider indirection
keeps layout testable headlessly.
