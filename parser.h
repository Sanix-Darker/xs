#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>   /* for size_t */

typedef struct ComputedStyle {
    char* width;       // e.g., "600px"
    char* height;      // e.g., "30px"
    char* background;  // e.g., "#FFCC00"
    char* text_align;  // e.g., "left", "center", or "right"
    char* font_size;   // e.g., "24px"
    char* color;       // e.g., "#333" or "red"
    char* margin;      // shorthand, e.g. "10px 20px"
    char* padding;     // shorthand
} ComputedStyle;

/* A single HTML attribute (MISS-001). Owned by the node. */
typedef struct {
    char* name;        // lowercased attribute name
    char* value;       // attribute value (may be empty string, never NULL)
} Attr;

typedef struct DOMNode {
    char* name;              // e.g., "div", "p", "#text", "h1", etc.
    char* text;              // content for text nodes
    char* href;              // link target for <a> tags (NULL otherwise)
    struct DOMNode** children;
    int children_count;
    int children_capacity;   // pre-allocated capacity for children array
    ComputedStyle* style;    // may be NULL if no style is applied

    Attr* attrs;             // attribute list (MISS-001), may be NULL
    int   attr_count;
    int   attr_capacity;
    struct DOMNode* parent;  // parent element (FEAT-010), NULL for root
    int   tag;               // interned TagId (FEAT-010); 0 = unknown
    int   arena_owned;       // 1 if allocated from a Document arena (MISS-007)
} DOMNode;

DOMNode* create_dom_node(const char* name, const char* text);
DOMNode* create_dom_node_n(const char* name, size_t name_len, const char* text);
void add_child(DOMNode* parent, DOMNode* child);
DOMNode* parse_html(const char* html);
void free_dom(DOMNode* node);
void split_text_nodes(DOMNode* node);
char* extract_style_text(DOMNode* root);

/* MISS-007: when an arena is set (non-NULL), all DOM allocations route through
   it; the tree is freed in one shot via arena_destroy (free_dom becomes a
   no-op for arena-owned nodes). Pass NULL to restore malloc-based allocation.
   `arena` is an opaque pointer to an Arena. */
void parser_set_arena(void* arena);

/* --- Attribute access (MISS-001) --- */
/* Add an attribute (copies name/value). name is lowercased. */
void        dom_add_attr(DOMNode* node, const char* name, const char* value);
/* Case-insensitive attribute lookup; returns value or NULL. */
const char* dom_attr(const DOMNode* node, const char* name);
/* Convenience: id attribute, or NULL. */
const char* dom_id(const DOMNode* node);
/* True if the node's class attribute contains the given class token. */
int         dom_has_class(const DOMNode* node, const char* cls);

#endif
