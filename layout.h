#ifndef LAYOUT_H
#define LAYOUT_H

#include "parser.h"
#include <stddef.h>      /* for size_t */
#include <stdio.h>       /* for FILE */

/* Rendering metadata passed from layout to render */
typedef struct {
    int font_size;      /* pt size (28, 24, 20, 18, 16, 15, 14, 13) */
    int is_heading;     /* 1-6 for h1-h6, 0 otherwise */
    int is_bold;        /* 1 if bold context */
    int is_italic;      /* 1 if italic context */
    int is_link;        /* 1 if href present */
    int is_list_item;   /* 1 if <li> marker box */
    int list_index;     /* 1+ for <ol>, 0 for <ul> bullet */
    int is_hr;          /* 1 for <hr> */
    int show_border;    /* 1 for wireframe structural elements */
    int is_pre;         /* 1 for <pre>/<code> background */
    int is_blockquote;  /* 1 for blockquote left bar */
    int is_image;       /* 1 for <img> box (MISS-002) */
} LayoutHints;

/* A single rectangle on the screen that represents one DOM node */
typedef struct {
    int      x, y, width, height;
    DOMNode *node;       /* pointer back to the DOM element */
    char    *href;       /* link target (not owned, points into DOMNode) */
    LayoutHints hints;   /* rendering metadata */
    int      aligned;    /* internal: line already shifted by alignment */
    /* Text run view (FEAT-011): for #text boxes, the box renders
       text[run_off .. run_off+run_len). run_len==0 means "whole node text". */
    int      run_off;
    int      run_len;
} LayoutBox;

/* Dynamic array of LayoutBox + bookkeeping information          */
typedef struct {
    LayoutBox *boxes;    /* contiguous buffer with the boxes     */
    size_t     count;    /* number of boxes currently in use     */
    size_t     capacity; /* boxes that fit in the allocated buf  */
    DOMNode   *dom;      /* (optional) pointer to the DOM tree   */
} Layout;

/* Font/measurement provider (FIX-008). Layout asks the provider to measure a
   string at the *actual* font size/style instead of scaling a single-size
   measurement. When provider (or its measure fn) is NULL, layout uses a
   deterministic headless approximation, which keeps --dump golden output
   machine-independent. */
typedef int (*MeasureTextFn)(void *ctx, const char *text,
                             int px, int bold, int italic);
typedef struct {
    void         *ctx;
    MeasureTextFn measure;
} FontProvider;

/* font: pass a FontProvider* (or NULL for deterministic approximation)
   window_w: actual window width in pixels                       */
Layout *layout_dom(DOMNode *root, const FontProvider *fonts, int window_w);
void    free_layout(Layout *layout);

/* Text zoom (MISS-009): multiply all resolved font sizes by `percent`/100.
   Clamped to a sane range. Default 100. Applies on the next layout_dom. */
void    layout_set_zoom(int percent);
int     layout_get_zoom(void);

/* Serialize a layout to a stable text format for golden testing (FEAT-003). */
void    dump_layout(const Layout *layout, FILE *out, int window_w, int window_h);

#endif /* LAYOUT_H */
