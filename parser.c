#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/usr/local/include/gumbo.h"  // because skill issues

// Create a DOM node with computed style pointer set to NULL.
DOMNode* create_dom_node(const char* name, const char* text) {
    DOMNode* node = (DOMNode*)malloc(sizeof(DOMNode));
    node->name = name ? strdup(name) : NULL;
    node->text = text ? strdup(text) : NULL;
    node->children = NULL;
    node->children_count = 0;
    node->style = NULL;
    return node;
}

static void parse_gumbo_node(GumboNode* gumbo_node, DOMNode* parent) {
    if (gumbo_node->type == GUMBO_NODE_ELEMENT) {
        GumboElement* element = &gumbo_node->v.element;
        const char* tag_name = gumbo_normalized_tagname(element->tag);
        if (strcmp(tag_name, "UNKNOWN") == 0) {
            tag_name = element->original_tag.data;
        }

        DOMNode* node = create_dom_node(tag_name, NULL);

        // Attach node to parent.
        if (parent) {
            parent->children = (DOMNode**)realloc(parent->children,
                sizeof(DOMNode*) * (parent->children_count + 1));
            parent->children[parent->children_count] = node;
            parent->children_count++;
        }

        // Recurse into children.
        GumboVector* children = &element->children;
        for (unsigned int i = 0; i < children->length; i++) {
            parse_gumbo_node(children->data[i], node);
        }
    } else if (gumbo_node->type == GUMBO_NODE_TEXT) {
        const char* text = gumbo_node->v.text.text;
        if (parent) {
            DOMNode* node = create_dom_node("#text", text);
            parent->children = (DOMNode**)realloc(parent->children,
                sizeof(DOMNode*) * (parent->children_count + 1));
            parent->children[parent->children_count] = node;
            parent->children_count++;
        }
    }
    // Comments, CDATA, etc. are ignored in this minimal example.
}

DOMNode* parse_html(const char* html) {
    GumboOutput* output = gumbo_parse(html);
    if (!output) {
        return NULL;
    }
    // Create a fake root.
    DOMNode* root = create_dom_node("root", NULL);
    parse_gumbo_node(output->root, root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return root;
}

void free_dom(DOMNode* node) {
    if (!node) return;
    if (node->name) free(node->name);
    if (node->text) free(node->text);
    // Free computed style if it exists.
    if (node->style) {
        if (node->style->width) free(node->style->width);
        if (node->style->height) free(node->style->height);
        if (node->style->background) free(node->style->background);
        free(node->style);
    }
    for (int i = 0; i < node->children_count; i++) {
        free_dom(node->children[i]);
    }
    free(node->children);
    free(node);
}
