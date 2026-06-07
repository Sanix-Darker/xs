#ifndef XS_UI_H
#define XS_UI_H

#include <stdint.h>

/* UI chrome helpers (FEAT-007). Pure geometry/theme logic lives here so it can
   be unit-tested without SDL. */

typedef struct {
    uint8_t r, g, b;
} RGB;

typedef struct {
    const char* name;
    RGB bg;         /* page background */
    RGB text;       /* body text */
    RGB heading;    /* heading text */
    RGB link;       /* link text */
    RGB bar_bg;     /* search bar background */
    RGB scrollbar;  /* scrollbar thumb */
} Theme;

/* Number of built-in themes. */
int          ui_theme_count(void);
/* Theme by index (wraps via modulo). */
const Theme* ui_theme(int index);

/* Scrollbar thumb geometry. Given the content height, the viewport height, and
   the current scroll_offset (0 at top, negative as you scroll down), compute
   the thumb's y and height within a track of [track_y, track_y+track_h].
   Returns 1 if a scrollbar is needed (content taller than viewport), else 0. */
int ui_scrollbar_thumb(int content_h, int view_h, int scroll_offset,
                       int track_y, int track_h, int* thumb_y, int* thumb_h);

/* Relative luminance (0..1) of an RGB color (WCAG-style). */
double ui_luminance(RGB c);
/* Contrast ratio between two colors (1..21). */
double ui_contrast_ratio(RGB a, RGB b);

/* Visited-URL set (FEAT-012): bounded, in-memory, session-only. */
void ui_visited_add(const char* url);
int  ui_visited_contains(const char* url);
void ui_visited_clear(void);

/* Case-insensitive substring search (FEAT-008 find-in-page). Returns 1 if
   `needle` occurs in `haystack` (empty needle -> 0). */
int ui_str_contains_ci(const char* haystack, const char* needle);

/* Compute a scroll_offset that centers a box at content-space y `box_y` of
   height `box_h` within a viewport of height `view_h`. Returns the clamped
   negative-or-zero offset (matching the renderer's convention). */
int ui_scroll_to_center(int box_y, int box_h, int view_h, int content_h);

#endif /* XS_UI_H */
