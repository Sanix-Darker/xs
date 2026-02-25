#ifndef LAYOUT_H
#define LAYOUT_H

#include "parser.h"
#include <stddef.h>      /* for size_t */

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
} LayoutHints;

/* A single rectangle on the screen that represents one DOM node */
typedef struct {
    int      x, y, width, height;
    DOMNode *node;       /* pointer back to the DOM element */
    char    *href;       /* link target (not owned, points into DOMNode) */
    LayoutHints hints;   /* rendering metadata */
} LayoutBox;

/* Dynamic array of LayoutBox + bookkeeping information          */
typedef struct {
    LayoutBox *boxes;    /* contiguous buffer with the boxes     */
    size_t     count;    /* number of boxes currently in use     */
    size_t     capacity; /* boxes that fit in the allocated buf  */
    DOMNode   *dom;      /* (optional) pointer to the DOM tree   */
} Layout;

/* font: pass TTF_Font* (or NULL for fallback approximation)
   window_w: actual window width in pixels                       */
Layout *layout_dom(DOMNode *root, void *font, int window_w);
void    free_layout(Layout *layout);

#endif /* LAYOUT_H */
