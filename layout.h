#ifndef LAYOUT_H
#define LAYOUT_H

#include "parser.h"
#include <stddef.h>      /* for size_t */

/* A single rectangle on the screen that represents one DOM node */
typedef struct {
    int      x, y, width, height;
    DOMNode *node;       /* pointer back to the DOM element */
} LayoutBox;

/* Dynamic array of LayoutBox + bookkeeping information          */
typedef struct {
    LayoutBox *boxes;    /* contiguous buffer with the boxes     */
    size_t     count;    /* number of boxes currently in use     */
    size_t     capacity; /* boxes that fit in the allocated buf  */
    DOMNode   *dom;      /* (optional) pointer to the DOM tree   */
} Layout;

Layout *layout_dom (DOMNode *root);
void     free_layout(Layout *layout);

#endif /* LAYOUT_H */
