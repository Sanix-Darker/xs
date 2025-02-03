#ifndef LAYOUT_H
#define LAYOUT_H

#include "parser.h"

typedef struct {
    int x, y, width, height;
    DOMNode *node;  // Pointer to the original DOM node.
} LayoutBox;

typedef struct {
    LayoutBox *boxes;
    int count;
    DOMNode *dom;   // Store pointer to the DOM used to generate this layout.
} Layout;

Layout* layout_dom(DOMNode* root);
void free_layout(Layout* layout);

#endif
