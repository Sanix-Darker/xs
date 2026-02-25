#ifndef PARSER_H
#define PARSER_H

typedef struct ComputedStyle {
    char* width;       // e.g., "600px"
    char* height;      // e.g., "30px"
    char* background;  // e.g., "#FFCC00"
    char* text_align;  // e.g., "left", "center", or "right"
    char* font_size;   // e.g., "24px"
} ComputedStyle;

typedef struct DOMNode {
    char* name;              // e.g., "div", "p", "#text", "h1", etc.
    char* text;              // content for text nodes
    char* href;              // link target for <a> tags (NULL otherwise)
    struct DOMNode** children;
    int children_count;
    int children_capacity;   // pre-allocated capacity for children array
    ComputedStyle* style;    // may be NULL if no style is applied
} DOMNode;

DOMNode* create_dom_node(const char* name, const char* text);
void add_child(DOMNode* parent, DOMNode* child);
DOMNode* parse_html(const char* html);
void free_dom(DOMNode* node);
void split_text_nodes(DOMNode* node);
char* extract_style_text(DOMNode* root);

#endif
