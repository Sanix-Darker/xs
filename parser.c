#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gumbo_src/gumbo.h"

/* --- Children management with pre-allocated capacity --- */

void add_child(DOMNode* parent, DOMNode* child) {
    if (!parent || !child) return;
    if (parent->children_count >= parent->children_capacity) {
        int newcap = parent->children_capacity ? parent->children_capacity * 2 : 4;
        DOMNode** tmp = realloc(parent->children, sizeof(DOMNode*) * newcap);
        if (!tmp) return;
        parent->children = tmp;
        parent->children_capacity = newcap;
    }
    parent->children[parent->children_count++] = child;
}

/* --- Node creation --- */

DOMNode* create_dom_node(const char* name, const char* text) {
    DOMNode* node = (DOMNode*)calloc(1, sizeof(DOMNode));
    node->name = name ? strdup(name) : NULL;
    node->text = text ? strdup(text) : NULL;
    return node;
}

/* --- Gumbo tree walker --- */

static void parse_gumbo_node(GumboNode* gumbo_node, DOMNode* parent) {
    if (gumbo_node->type == GUMBO_NODE_ELEMENT) {
        GumboElement* element = &gumbo_node->v.element;
        const char* tag_name = gumbo_normalized_tagname(element->tag);
        if (strcmp(tag_name, "UNKNOWN") == 0) {
            tag_name = element->original_tag.data;
        }

        DOMNode* node = create_dom_node(tag_name, NULL);

        /* Extract href from <a> tags */
        if (strcasecmp(tag_name, "a") == 0) {
            GumboAttribute* attr = gumbo_get_attribute(&element->attributes, "href");
            if (attr && attr->value) {
                node->href = strdup(attr->value);
            }
        }

        if (parent) {
            add_child(parent, node);
        }

        GumboVector* children = &element->children;
        for (unsigned int i = 0; i < children->length; i++) {
            parse_gumbo_node(children->data[i], node);
        }
    } else if (gumbo_node->type == GUMBO_NODE_TEXT) {
        const char* text = gumbo_node->v.text.text;
        if (parent) {
            DOMNode* node = create_dom_node("#text", text);
            add_child(parent, node);
        }
    }
}

/* --- Public API --- */

DOMNode* parse_html(const char* html) {
    GumboOutput* output = gumbo_parse(html);
    if (!output) {
        return NULL;
    }
    DOMNode* root = create_dom_node("root", NULL);
    parse_gumbo_node(output->root, root);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return root;
}

void free_dom(DOMNode* node) {
    if (!node) return;
    if (node->name) free(node->name);
    if (node->text) free(node->text);
    if (node->href) free(node->href);
    if (node->style) {
        if (node->style->width) free(node->style->width);
        if (node->style->height) free(node->style->height);
        if (node->style->background) free(node->style->background);
        if (node->style->text_align) free(node->style->text_align);
        if (node->style->font_size) free(node->style->font_size);
        free(node->style);
    }
    for (int i = 0; i < node->children_count; i++) {
        free_dom(node->children[i]);
    }
    free(node->children);
    free(node);
}

/* --- Style text extraction (collect all <style> tag contents) --- */

static void collect_style_text(DOMNode* node, char** buf, size_t* len, size_t* cap) {
    if (!node) return;
    if (node->name && strcasecmp(node->name, "style") == 0) {
        for (int i = 0; i < node->children_count; i++) {
            const char* t = node->children[i]->text;
            if (!t) continue;
            size_t tlen = strlen(t);
            while (*len + tlen + 2 > *cap) {
                *cap = *cap ? *cap * 2 : 256;
                *buf = realloc(*buf, *cap);
            }
            memcpy(*buf + *len, t, tlen);
            *len += tlen;
            (*buf)[(*len)++] = '\n';
            (*buf)[*len] = '\0';
        }
        return;
    }
    for (int i = 0; i < node->children_count; i++) {
        collect_style_text(node->children[i], buf, len, cap);
    }
}

/* --- Split text nodes into individual words for wrapping --- */

void split_text_nodes(DOMNode* node) {
    if (!node) return;

    for (int i = 0; i < node->children_count; i++) {
        DOMNode* child = node->children[i];
        if (!child || !child->name) continue;

        if (strcmp(child->name, "#text") == 0 && child->text && strchr(child->text, ' ')) {
            /* Split this text node into word nodes */
            const char* src = child->text;

            /* Skip leading whitespace */
            while (*src && *src == ' ') src++;
            if (!*src) { split_text_nodes(child); continue; }

            /* Count words */
            int nwords = 0;
            const char* p = src;
            while (*p) {
                while (*p && *p != ' ') p++;
                nwords++;
                while (*p == ' ') p++;
            }
            if (nwords <= 1) { split_text_nodes(child); continue; }

            /* Build word nodes */
            DOMNode** words = malloc(sizeof(DOMNode*) * nwords);
            if (!words) continue;
            p = src;
            int wi = 0;
            while (*p) {
                const char* start = p;
                while (*p && *p != ' ') p++;
                int len = (int)(p - start);
                char* word = malloc(len + 1);
                memcpy(word, start, len);
                word[len] = '\0';
                words[wi] = create_dom_node("#text", word);
                free(word);
                wi++;
                while (*p == ' ') p++;
            }

            /* Replace this child in parent's children array with the word nodes */
            int new_count = node->children_count - 1 + nwords;
            DOMNode** new_children = malloc(sizeof(DOMNode*) * new_count);
            int ni = 0;
            for (int j = 0; j < node->children_count; j++) {
                if (j == i) {
                    for (int k = 0; k < nwords; k++)
                        new_children[ni++] = words[k];
                } else {
                    new_children[ni++] = node->children[j];
                }
            }
            free(words);

            /* Free old text node and swap arrays */
            free_dom(child);
            free(node->children);
            node->children = new_children;
            node->children_count = new_count;
            node->children_capacity = new_count;

            /* Re-examine from current position (new words don't need splitting) */
            i += nwords - 1;
            continue;
        }

        split_text_nodes(child);
    }
}

char* extract_style_text(DOMNode* root) {
    char* buf = NULL;
    size_t len = 0, cap = 0;
    collect_style_text(root, &buf, &len, &cap);
    return buf;
}
