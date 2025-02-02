#ifndef LAYOUT_H
#define LAYOUT_H

#include "parser.h"

typedef struct {
    int x, y, width, height;
    DOMNode* node;
} LayoutBox;

typedef struct {
    LayoutBox* boxes;
    int count;
} Layout;

Layout* layout_dom(DOMNode* root);
void free_layout(Layout* layout);

#endif
