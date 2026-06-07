#include "select.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static void copy_name(char* dst, const char* start, size_t len) {
    if (len >= 64) len = 63;
    for (size_t i = 0; i < len; i++)
        dst[i] = (char)xs_tolower((unsigned char)start[i]);
    dst[len] = '\0';
}

/* Parse one compound selector (no whitespace inside), e.g. "p.note#x" or "*". */
static int parse_compound(const char* s, const char* end, CompoundSel* out) {
    out->n_simples = 0;
    const char* p = s;
    while (p < end && out->n_simples < SEL_MAX_SIMPLE) {
        SimpleSel* simp = &out->simples[out->n_simples];
        if (*p == '.') {
            p++;
            const char* st = p;
            while (p < end && *p != '.' && *p != '#') p++;
            if (p == st) return 0;
            simp->kind = SIMPLE_CLASS;
            copy_name(simp->name, st, (size_t)(p - st));
            out->n_simples++;
        } else if (*p == '#') {
            p++;
            const char* st = p;
            while (p < end && *p != '.' && *p != '#') p++;
            if (p == st) return 0;
            simp->kind = SIMPLE_ID;
            copy_name(simp->name, st, (size_t)(p - st));
            out->n_simples++;
        } else if (*p == '*') {
            p++;
            simp->kind = SIMPLE_UNIVERSAL;
            simp->name[0] = '\0';
            out->n_simples++;
        } else {
            /* type selector */
            const char* st = p;
            while (p < end && *p != '.' && *p != '#' && *p != '*') p++;
            if (p == st) return 0;
            simp->kind = SIMPLE_TYPE;
            copy_name(simp->name, st, (size_t)(p - st));
            out->n_simples++;
        }
    }
    return out->n_simples > 0;
}

/* Parse one complex selector (descendant chain). */
static void parse_complex(const char* s, const char* end, ComplexSel* out) {
    out->n_compounds = 0;
    out->valid = 0;
    const char* p = s;
    while (p < end) {
        while (p < end && xs_is_html_space((unsigned char)*p)) p++;
        if (p >= end) break;
        const char* tok = p;
        while (p < end && !xs_is_html_space((unsigned char)*p)) p++;
        if (out->n_compounds >= SEL_MAX_COMPOUND) return;  /* too deep -> invalid */
        if (!parse_compound(tok, p, &out->compounds[out->n_compounds])) return;
        out->n_compounds++;
    }
    if (out->n_compounds > 0) {
        out->valid = 1;
        out->specificity = selector_specificity(out);
    }
}

int selector_specificity(const ComplexSel* sel) {
    int ids = 0, classes = 0, types = 0;
    for (int c = 0; c < sel->n_compounds; c++) {
        const CompoundSel* comp = &sel->compounds[c];
        for (int i = 0; i < comp->n_simples; i++) {
            switch (comp->simples[i].kind) {
            case SIMPLE_ID:        ids++; break;
            case SIMPLE_CLASS:     classes++; break;
            case SIMPLE_TYPE:      types++; break;
            case SIMPLE_UNIVERSAL: break;  /* contributes 0 */
            }
        }
    }
    return ids * 10000 + classes * 100 + types;
}

SelectorList* selector_list_parse(const char* text) {
    SelectorList* list = xs_malloc(sizeof *list);
    if (!list) return NULL;
    list->items = NULL;
    list->count = 0;
    if (!text) return list;

    const char* p = text;
    while (*p) {
        const char* start = p;
        while (*p && *p != ',') p++;
        const char* end = p;
        if (*p == ',') p++;

        ComplexSel sel;
        memset(&sel, 0, sizeof sel);
        parse_complex(start, end, &sel);

        ComplexSel* tmp = xs_realloc(list->items, sizeof(ComplexSel) * (size_t)(list->count + 1));
        if (!tmp) break;
        list->items = tmp;
        list->items[list->count++] = sel;
    }
    return list;
}

void selector_list_free(SelectorList* list) {
    if (!list) return;
    free(list->items);
    free(list);
}

/* Match a single compound against a single element node. */
static int compound_matches(const CompoundSel* comp, const DOMNode* node) {
    if (!node || !node->name) return 0;
    if (node->name[0] == '#') return 0;  /* not an element (e.g. #text) */
    for (int i = 0; i < comp->n_simples; i++) {
        const SimpleSel* s = &comp->simples[i];
        switch (s->kind) {
        case SIMPLE_UNIVERSAL:
            break;
        case SIMPLE_TYPE:
            if (strcmp(node->name, s->name) != 0) return 0;
            break;
        case SIMPLE_CLASS:
            if (!dom_has_class(node, s->name)) return 0;
            break;
        case SIMPLE_ID: {
            const char* id = dom_id(node);
            if (!id || strcmp(id, s->name) != 0) return 0;
            break;
        }
        }
    }
    return 1;
}

int selector_matches(const ComplexSel* sel, const DOMNode* node) {
    if (!sel || !sel->valid || sel->n_compounds == 0) return 0;

    /* Rightmost compound must match the node itself. */
    int last = sel->n_compounds - 1;
    if (!compound_matches(&sel->compounds[last], node)) return 0;

    /* Walk ancestors to satisfy earlier compounds (descendant combinator). */
    int ci = last - 1;
    const DOMNode* anc = node->parent;
    while (ci >= 0) {
        int matched = 0;
        while (anc) {
            if (compound_matches(&sel->compounds[ci], anc)) { matched = 1; anc = anc->parent; break; }
            anc = anc->parent;
        }
        if (!matched) return 0;
        ci--;
    }
    return 1;
}
