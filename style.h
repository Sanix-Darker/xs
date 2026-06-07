#ifndef XS_STYLE_H
#define XS_STYLE_H

#include <stdint.h>

/* Typed CSS values (FIX-007, FEAT-005). */

typedef enum {
    LEN_AUTO = 0,   /* unspecified / auto */
    LEN_PX,
    LEN_PERCENT,
    LEN_EM,
} LengthUnit;

typedef struct {
    float      value;
    LengthUnit unit;
} Length;

typedef struct {
    uint8_t r, g, b, a;   /* a==0 means transparent/unset for backgrounds */
} Color;

typedef struct {
    Length top, right, bottom, left;
} Edge;

/* text-align values */
enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2, ALIGN_JUSTIFY = 3 };

/* display values (subset) */
enum { DISP_INLINE = 0, DISP_BLOCK = 1, DISP_NONE = 2, DISP_LIST_ITEM = 3 };

/* Parse a CSS length like "12px", "50%", "1.5em", "auto", unitless. */
Length style_parse_length(const char* s);

/* Resolve a length to pixels against a containing size and current font size.
   For LEN_AUTO returns `auto_value`. */
int style_resolve_length(Length len, int containing_px, int font_px, int auto_value);

/* Parse a CSS color: #rgb, #rrggbb, rgb()/rgba(), or a named color.
   Returns 1 on success (fills *out), 0 if unrecognized (out unchanged). */
int style_parse_color(const char* s, Color* out);

/* Parse text-align keyword; returns ALIGN_* (default ALIGN_LEFT). */
int style_parse_align(const char* s);

/* Parse a 1-4 value edge shorthand (e.g. "10px", "10px 20px",
   "1px 2px 3px 4px") into top/right/bottom/left lengths (CSS order). */
void style_parse_edge(const char* s, Edge* out);

#endif /* XS_STYLE_H */
