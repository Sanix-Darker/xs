#include "style.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

Length style_parse_length(const char* s) {
    Length len = { 0.0f, LEN_AUTO };
    if (!s) return len;
    while (*s && xs_is_html_space((unsigned char)*s)) s++;
    if (!*s) return len;

    if (strcasecmp(s, "auto") == 0) { len.unit = LEN_AUTO; return len; }

    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s) return len;   /* no number -> auto */

    /* skip spaces before unit */
    while (*end && xs_is_html_space((unsigned char)*end)) end++;

    if (end[0] == '%') {
        len.unit = LEN_PERCENT;
    } else if ((end[0] == 'e' || end[0] == 'E') &&
               (end[1] == 'm' || end[1] == 'M')) {
        len.unit = LEN_EM;
    } else if ((end[0] == 'r' || end[0] == 'R') &&
               (end[1] == 'e' || end[1] == 'E') &&
               (end[2] == 'm' || end[2] == 'M')) {
        len.unit = LEN_EM;  /* treat rem as em (no root tracking yet) */
    } else if ((end[0] == 'p' || end[0] == 'P') &&
               (end[1] == 't' || end[1] == 'T')) {
        len.unit = LEN_PX;
        v = v * 96.0 / 72.0;  /* pt -> px */
    } else {
        /* px or unitless -> treat as px */
        len.unit = LEN_PX;
    }
    len.value = (float)v;
    return len;
}

int style_resolve_length(Length len, int containing_px, int font_px, int auto_value) {
    switch (len.unit) {
    case LEN_PX:      return (int)(len.value + 0.5f);
    case LEN_PERCENT: return (int)(len.value / 100.0f * (float)containing_px + 0.5f);
    case LEN_EM:      return (int)(len.value * (float)font_px + 0.5f);
    case LEN_AUTO:
    default:          return auto_value;
    }
}

int style_parse_align(const char* s) {
    if (!s) return ALIGN_LEFT;
    if (strcasecmp(s, "center") == 0) return ALIGN_CENTER;
    if (strcasecmp(s, "right") == 0)  return ALIGN_RIGHT;
    if (strcasecmp(s, "justify") == 0) return ALIGN_JUSTIFY;
    return ALIGN_LEFT;
}

void style_parse_edge(const char* s, Edge* out) {
    Edge e = {{0,LEN_AUTO},{0,LEN_AUTO},{0,LEN_AUTO},{0,LEN_AUTO}};
    *out = e;
    if (!s) return;
    Length vals[4];
    int n = 0;
    const char* p = s;
    while (*p && n < 4) {
        while (*p && xs_is_html_space((unsigned char)*p)) p++;
        if (!*p) break;
        const char* start = p;
        while (*p && !xs_is_html_space((unsigned char)*p)) p++;
        char tmp[32];
        size_t len = (size_t)(p - start);
        if (len >= sizeof tmp) len = sizeof tmp - 1;
        memcpy(tmp, start, len); tmp[len] = '\0';
        vals[n++] = style_parse_length(tmp);
    }
    if (n == 0) return;
    /* CSS shorthand expansion */
    if (n == 1) { out->top = out->right = out->bottom = out->left = vals[0]; }
    else if (n == 2) { out->top = out->bottom = vals[0]; out->right = out->left = vals[1]; }
    else if (n == 3) { out->top = vals[0]; out->right = out->left = vals[1]; out->bottom = vals[2]; }
    else { out->top = vals[0]; out->right = vals[1]; out->bottom = vals[2]; out->left = vals[3]; }
}

/* --- Color parsing --- */

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = xs_tolower(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

struct NamedColor { const char* name; uint8_t r, g, b; };
static const struct NamedColor named_colors[] = {
    { "black",   0,   0,   0   },
    { "white",   255, 255, 255 },
    { "red",     255, 0,   0   },
    { "green",   0,   128, 0   },
    { "blue",    0,   0,   255 },
    { "yellow",  255, 255, 0   },
    { "gray",    128, 128, 128 },
    { "grey",    128, 128, 128 },
    { "silver",  192, 192, 192 },
    { "maroon",  128, 0,   0   },
    { "navy",    0,   0,   128 },
    { "olive",   128, 128, 0   },
    { "purple",  128, 0,   128 },
    { "teal",    0,   128, 128 },
    { "aqua",    0,   255, 255 },
    { "fuchsia", 255, 0,   255 },
    { "orange",  255, 165, 0   },
    { "lime",    0,   255, 0   },
};

int style_parse_color(const char* s, Color* out) {
    if (!s || !out) return 0;
    while (*s && xs_is_html_space((unsigned char)*s)) s++;

    if (*s == '#') {
        const char* h = s + 1;
        size_t n = 0;
        while (hexval((unsigned char)h[n]) >= 0) n++;
        if (n == 3) {
            int r = hexval(h[0]), g = hexval(h[1]), b = hexval(h[2]);
            out->r = (uint8_t)(r * 17); out->g = (uint8_t)(g * 17);
            out->b = (uint8_t)(b * 17); out->a = 255;
            return 1;
        } else if (n == 6) {
            out->r = (uint8_t)(hexval(h[0]) * 16 + hexval(h[1]));
            out->g = (uint8_t)(hexval(h[2]) * 16 + hexval(h[3]));
            out->b = (uint8_t)(hexval(h[4]) * 16 + hexval(h[5]));
            out->a = 255;
            return 1;
        }
        return 0;
    }

    if (strncasecmp(s, "rgb", 3) == 0) {
        const char* p = strchr(s, '(');
        if (!p) return 0;
        p++;
        int vals[4] = { 0, 0, 0, 255 };
        int nv = 0;
        while (*p && *p != ')' && nv < 4) {
            while (*p && (xs_is_html_space((unsigned char)*p) || *p == ',')) p++;
            if (*p == ')' || !*p) break;
            double v = strtod(p, (char**)&p);
            /* alpha (4th) is 0..1 float; others 0..255 */
            if (nv == 3) vals[nv] = (int)(v * 255.0 + 0.5);
            else         vals[nv] = (int)(v + 0.5);
            nv++;
            while (*p && *p != ',' && *p != ')') p++;
        }
        if (nv < 3) return 0;
        for (int i = 0; i < 4; i++) { if (vals[i] < 0) vals[i] = 0; if (vals[i] > 255) vals[i] = 255; }
        out->r = (uint8_t)vals[0]; out->g = (uint8_t)vals[1];
        out->b = (uint8_t)vals[2]; out->a = (uint8_t)vals[3];
        return 1;
    }

    if (strcasecmp(s, "transparent") == 0) {
        out->r = out->g = out->b = 0; out->a = 0;
        return 1;
    }

    for (size_t i = 0; i < sizeof(named_colors)/sizeof(named_colors[0]); i++) {
        if (strcasecmp(s, named_colors[i].name) == 0) {
            out->r = named_colors[i].r; out->g = named_colors[i].g;
            out->b = named_colors[i].b; out->a = 255;
            return 1;
        }
    }
    return 0;
}
