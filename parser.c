#include "parser.h"
#include "util.h"
#include "tagid.h"
#include "arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "gumbo_src/gumbo.h"

/* MISS-007: optional current parse arena. When set, DOM allocations come from
   it and the whole tree is freed via arena_destroy (free_dom no-ops). */
static Arena *g_parse_arena = NULL;
void parser_set_arena(void *arena) { g_parse_arena = (Arena *)arena; }

static void *pa_alloc(size_t n) {
    return g_parse_arena ? arena_alloc(g_parse_arena, n) : xs_malloc(n);
}
static void *pa_calloc(size_t n) {
    if (g_parse_arena) return arena_calloc(g_parse_arena, 1, n);
    return xs_calloc(1, n);
}
static char *pa_strdup(const char *s) {
    return g_parse_arena ? arena_strdup(g_parse_arena, s) : xs_strdup(s);
}
static char *pa_strndup(const char *s, size_t n) {
    return g_parse_arena ? arena_strndup(g_parse_arena, s, n) : xs_strndup(s, n);
}
static void *pa_realloc(void *p, size_t old_n, size_t new_n) {
    return g_parse_arena ? arena_realloc(g_parse_arena, p, old_n, new_n)
                         : xs_realloc(p, new_n);
}
static int pa_owned(void) { return g_parse_arena != NULL; }

/* --- Children management with pre-allocated capacity --- */

void add_child(DOMNode* parent, DOMNode* child) {
    if (!parent || !child) return;
    if (parent->children_count >= parent->children_capacity) {
        int oldcap = parent->children_capacity;
        int newcap = oldcap ? oldcap * 2 : 4;
        DOMNode** tmp = pa_realloc(parent->children,
                                   sizeof(DOMNode*) * (size_t)oldcap,
                                   sizeof(DOMNode*) * (size_t)newcap);
        if (!tmp) return;
        parent->children = tmp;
        parent->children_capacity = newcap;
    }
    child->parent = parent;       /* FEAT-010: parent pointer */
    parent->children[parent->children_count++] = child;
}

/* --- Attribute access (MISS-001) --- */

void dom_add_attr(DOMNode* node, const char* name, const char* value) {
    if (!node || !name) return;
    if (node->attr_count >= node->attr_capacity) {
        int oldcap = node->attr_capacity;
        int newcap = oldcap ? oldcap * 2 : 4;
        Attr* tmp = pa_realloc(node->attrs, sizeof(Attr) * (size_t)oldcap,
                               sizeof(Attr) * (size_t)newcap);
        if (!tmp) return;
        node->attrs = tmp;
        node->attr_capacity = newcap;
    }
    char* n = pa_strdup(name);
    if (!n) return;
    for (char* p = n; *p; ++p) *p = (char)xs_tolower((unsigned char)*p);
    char* v = pa_strdup(value ? value : "");
    if (!v) { if (!pa_owned()) free(n); return; }
    node->attrs[node->attr_count].name = n;
    node->attrs[node->attr_count].value = v;
    node->attr_count++;
}

const char* dom_attr(const DOMNode* node, const char* name) {
    if (!node || !name) return NULL;
    for (int i = 0; i < node->attr_count; i++) {
        const char* a = node->attrs[i].name;
        const char* b = name;
        /* case-insensitive compare */
        while (*a && *b &&
               xs_tolower((unsigned char)*a) == xs_tolower((unsigned char)*b)) {
            a++; b++;
        }
        if (*a == '\0' && *b == '\0') return node->attrs[i].value;
    }
    return NULL;
}

const char* dom_id(const DOMNode* node) {
    return dom_attr(node, "id");
}

int dom_has_class(const DOMNode* node, const char* cls) {
    if (!node || !cls || !*cls) return 0;
    const char* classes = dom_attr(node, "class");
    if (!classes) return 0;
    size_t clen = strlen(cls);
    const char* p = classes;
    while (*p) {
        while (*p && xs_is_html_space((unsigned char)*p)) p++;
        const char* start = p;
        while (*p && !xs_is_html_space((unsigned char)*p)) p++;
        size_t tlen = (size_t)(p - start);
        if (tlen == clen && strncmp(start, cls, clen) == 0) return 1;
    }
    return 0;
}

/* --- Node creation --- */

DOMNode* create_dom_node(const char* name, const char* text) {
    return create_dom_node_n(name, name ? strlen(name) : 0, text);
}

/* Create a node copying exactly name_len bytes of `name` (which need not be
   NUL-terminated, e.g. Gumbo's original_tag string piece — FIX-001). The tag
   name is lowercased for canonical, case-insensitive handling. */
DOMNode* create_dom_node_n(const char* name, size_t name_len, const char* text) {
    DOMNode* node = (DOMNode*)pa_calloc(sizeof(DOMNode));
    if (!node) return NULL;
    node->arena_owned = pa_owned();
    if (name) {
        char* n = pa_strndup(name, name_len);
        if (n) {
            for (char* p = n; *p; ++p) *p = (char)xs_tolower((unsigned char)*p);
        }
        node->name = n;
        node->tag = n ? (int)tagid_from_name(n) : TAG_UNKNOWN;
    }
    if (text) {
        node->text = pa_strdup(text);
    }
    return node;
}

/* --- Gumbo tree walker --- */

static void parse_gumbo_node(GumboNode* gumbo_node, DOMNode* parent,
                             int depth, unsigned* node_count) {
    /* FIX-015: bound recursion depth and total node count against hostile input. */
    if (depth > XS_MAX_DEPTH) return;
    if (*node_count >= XS_MAX_NODES) return;

    if (gumbo_node->type == GUMBO_NODE_ELEMENT) {
        GumboElement* element = &gumbo_node->v.element;
        const char* tag_name = gumbo_normalized_tagname(element->tag);
        size_t tag_len;

        /* FIX-001: unknown tags (custom elements, SVG/MathML, etc.) map to an
           empty normalized name; their real name lives in original_tag, which
           is a string piece that is NOT NUL-terminated, so it must be copied
           with its explicit length. */
        if (element->tag == GUMBO_TAG_UNKNOWN || !tag_name || tag_name[0] == '\0') {
            if (element->original_tag.length > 0 && element->original_tag.data) {
                /* original_tag includes the surrounding "<...>" and any
                   attributes; normalize it to just the tag-name span. The
                   string piece is NOT NUL-terminated, so copy with length. */
                GumboStringPiece piece = element->original_tag;
                gumbo_tag_from_original_text(&piece);
                if (piece.length > 0 && piece.data) {
                    tag_name = piece.data;
                    tag_len  = piece.length;
                } else {
                    tag_name = "unknown";
                    tag_len  = 7;
                }
            } else {
                tag_name = "unknown";
                tag_len  = 7;
            }
        } else {
            tag_len = strlen(tag_name);
        }

        DOMNode* node = create_dom_node_n(tag_name, tag_len, NULL);
        if (!node) return;
        (*node_count)++;

        /* Capture all attributes (MISS-001). Gumbo attribute names/values are
           NUL-terminated C strings owned by Gumbo. */
        GumboVector* attrs = &element->attributes;
        for (unsigned int i = 0; i < attrs->length; i++) {
            GumboAttribute* ga = (GumboAttribute*)attrs->data[i];
            if (ga && ga->name)
                dom_add_attr(node, ga->name, ga->value ? ga->value : "");
        }

        /* Keep the cached href pointer for hot link rendering. */
        if (node->name && strcmp(node->name, "a") == 0) {
            const char* hv = dom_attr(node, "href");
            if (hv) node->href = xs_strdup(hv);
        }

        if (parent) {
            add_child(parent, node);
        }

        GumboVector* children = &element->children;
        for (unsigned int i = 0; i < children->length; i++) {
            parse_gumbo_node(children->data[i], node, depth + 1, node_count);
        }
    } else if (gumbo_node->type == GUMBO_NODE_TEXT ||
               gumbo_node->type == GUMBO_NODE_CDATA) {
        const char* text = gumbo_node->v.text.text;
        if (parent && text) {
            DOMNode* node = create_dom_node("#text", text);
            if (node) {
                (*node_count)++;
                add_child(parent, node);
            }
        }
    }
}

/* --- Public API --- */

DOMNode* parse_html(const char* html) {
    if (!html) return NULL;
    GumboOutput* output = gumbo_parse(html);
    if (!output) {
        return NULL;
    }
    DOMNode* root = create_dom_node("root", NULL);
    if (!root) {
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        return NULL;
    }
    unsigned node_count = 1;
    parse_gumbo_node(output->root, root, 0, &node_count);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return root;
}

void free_dom(DOMNode* node) {
    if (!node) return;
    /* MISS-007: arena-owned trees are freed wholesale via arena_destroy. */
    if (node->arena_owned) return;
    if (node->name) free(node->name);
    if (node->text) free(node->text);
    if (node->href) free(node->href);
    if (node->attrs) {
        for (int i = 0; i < node->attr_count; i++) {
            free(node->attrs[i].name);
            free(node->attrs[i].value);
        }
        free(node->attrs);
    }
    if (node->style) {
        if (node->style->width) free(node->style->width);
        if (node->style->height) free(node->style->height);
        if (node->style->background) free(node->style->background);
        if (node->style->text_align) free(node->style->text_align);
        if (node->style->font_size) free(node->style->font_size);
        if (node->style->color) free(node->style->color);
        if (node->style->margin) free(node->style->margin);
        if (node->style->padding) free(node->style->padding);
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
    if (node->name && strcmp(node->name, "style") == 0) {
        for (int i = 0; i < node->children_count; i++) {
            const char* t = node->children[i]->text;
            if (!t) continue;
            size_t tlen = strlen(t);
            while (*len + tlen + 2 > *cap) {
                size_t newcap = *cap ? *cap * 2 : 256;
                char* tmp = xs_realloc(*buf, newcap);
                if (!tmp) return;   /* keep what we have on failure */
                *buf = tmp;
                *cap = newcap;
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

/* Raw-text / whitespace-significant elements whose text must NOT be split into
   words: doing so corrupts script/style source (FIX-002) and destroys the
   significant whitespace of <pre>/<textarea> (FIX-003). */
static int is_rawtext_element(const char* name) {
    if (!name) return 0;
    return strcmp(name, "script") == 0 || strcmp(name, "style") == 0 ||
           strcmp(name, "pre") == 0 || strcmp(name, "textarea") == 0 ||
           strcmp(name, "title") == 0;
}

/* --- Split text nodes into individual words for wrapping --- */

void split_text_nodes(DOMNode* node) {
    if (!node) return;

    /* FIX-002/FIX-003: never split inside raw-text/whitespace-significant
       elements. Their text content is consumed verbatim elsewhere. */
    if (is_rawtext_element(node->name)) return;

    for (int i = 0; i < node->children_count; i++) {
        DOMNode* child = node->children[i];
        if (!child || !child->name) continue;

        /* Determine whether this text node contains any html-space to split on. */
        int has_space = 0;
        if (strcmp(child->name, "#text") == 0 && child->text) {
            for (const char* q = child->text; *q; ++q) {
                if (xs_is_html_space((unsigned char)*q)) { has_space = 1; break; }
            }
        }

        if (has_space) {
            /* Split this text node into word nodes, treating ANY run of
               html-space (space/tab/newline/CR/FF) as a single delimiter
               (FIX-003). */
            const char* src = child->text;

            /* Skip leading whitespace */
            while (*src && xs_is_html_space((unsigned char)*src)) src++;
            if (!*src) { split_text_nodes(child); continue; }

            /* Count words */
            int nwords = 0;
            const char* p = src;
            while (*p) {
                while (*p && !xs_is_html_space((unsigned char)*p)) p++;
                nwords++;
                while (*p && xs_is_html_space((unsigned char)*p)) p++;
            }
            if (nwords <= 1) { split_text_nodes(child); continue; }

            /* Build word nodes */
            DOMNode** words = xs_malloc(sizeof(DOMNode*) * (size_t)nwords);
            if (!words) { split_text_nodes(child); continue; }
            p = src;
            int wi = 0;
            while (*p && wi < nwords) {
                const char* start = p;
                while (*p && !xs_is_html_space((unsigned char)*p)) p++;
                size_t len = (size_t)(p - start);
                DOMNode* wnode = create_dom_node_n("#text", 5, NULL);
                if (wnode) {
                    wnode->text = xs_strndup(start, len);
                    wnode->parent = node;
                }
                words[wi++] = wnode;
                while (*p && xs_is_html_space((unsigned char)*p)) p++;
            }
            nwords = wi;  /* actual produced */

            /* Replace this child in parent's children array with the word nodes */
            int new_count = node->children_count - 1 + nwords;
            DOMNode** new_children = xs_malloc(sizeof(DOMNode*) * (size_t)new_count);
            if (!new_children) {
                /* Recover: drop the split, free the temp word nodes. */
                for (int k = 0; k < nwords; k++) free_dom(words[k]);
                free(words);
                split_text_nodes(child);
                continue;
            }
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
