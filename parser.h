#ifndef PARSER_H
#define PARSER_H

typedef struct ComputedStyle {
    char* width;       // e.g., "600px"
    char* height;      // e.g., "30px"
    char* background;  // e.g., "#FFCC00"
    char* text_align;  // e.g., "left", "center", or "right"
} ComputedStyle;

typedef struct DOMNode {
    char* name;              // e.g., "div", "p", "#text", "h1", etc.
    char* text;              // content for text nodes
    struct DOMNode** children;
    int children_count;
    ComputedStyle* style;    // may be NULL if no style is applied
} DOMNode;

DOMNode* parse_html(const char* html);
void free_dom(DOMNode* node);

#endif
