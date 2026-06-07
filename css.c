#include "css.h"
#include "util.h"
#include "select.h"
#include "style.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// --- Utility Functions ---

// Trim whitespace from start and end of a string. Returns a new allocated string.
static char* trim(const char* str) {
    while (xs_isspace((unsigned char)*str)) str++;
    int len = strlen(str);
    while (len > 0 && xs_isspace((unsigned char)str[len-1])) len--;
    char* result = xs_malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

// --- CSS Parsing ---

// Skip a run of whitespace and /* ... */ comments (FIX-006). Handles
// unterminated comments by running to EOF. Advances *pp past all of them.
static void skip_ws_and_comments(const char** pp) {
    const char* p = *pp;
    for (;;) {
        while (*p && xs_isspace((unsigned char)*p)) p++;
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;   /* skip closing */ /* else: unterminated -> EOF */
        } else {
            break;
        }
    }
    *pp = p;
}

// Copy [start, end) into a freshly trimmed string, stripping any embedded
// comments. Returns NULL on allocation failure.
static char* trim_strip_comments(const char* start, const char* end) {
    size_t raw = (size_t)(end - start);
    char* tmp = xs_malloc(raw + 1);
    if (!tmp) return NULL;
    size_t n = 0;
    const char* p = start;
    while (p < end) {
        if (p[0] == '/' && p + 1 < end && p[1] == '*') {
            p += 2;
            while (p < end && !(p[0] == '*' && p + 1 < end && p[1] == '/')) p++;
            if (p < end) p += 2;
            continue;
        }
        tmp[n++] = *p++;
    }
    tmp[n] = '\0';
    char* result = trim(tmp);
    free(tmp);
    return result;
}

// This is a pragmatic flat parser for: selector { prop: val; ... }
// Comment-aware (FIX-006) and allocation-checked (FIX-005).
CSSStyleSheet* parse_css(const char* css_text) {
    if (!css_text) return NULL;
    CSSStyleSheet* sheet = xs_malloc(sizeof(CSSStyleSheet));
    if (!sheet) return NULL;
    sheet->rules = NULL;
    sheet->rule_count = 0;

    const char* p = css_text;
    while (*p) {
        skip_ws_and_comments(&p);
        if (!*p) break;

        // Read selector until '{' (comments inside are stripped on copy).
        const char* sel_start = p;
        while (*p && *p != '{') p++;
        if (*p != '{') break; // end if no '{'
        char* trimmed_selector = trim_strip_comments(sel_start, p);
        if (!trimmed_selector) break;

        p++; // skip '{'
        // Parse declarations until '}'
        CSSDeclaration* declarations = NULL;
        int decl_count = 0;
        for (;;) {
            skip_ws_and_comments(&p);
            if (!*p || *p == '}') break;

            // Read property name until ':' or '}'
            const char* prop_start = p;
            while (*p && *p != ':' && *p != '}') p++;
            if (*p != ':') break;
            char* trimmed_property = trim_strip_comments(prop_start, p);
            p++; // skip ':'

            // Read value until ';' or '}'
            const char* val_start = p;
            while (*p && *p != ';' && *p != '}') p++;
            char* trimmed_value = trim_strip_comments(val_start, p);
            if (*p == ';') p++; // skip ';'

            if (!trimmed_property || !trimmed_value) {
                free(trimmed_property);
                free(trimmed_value);
                continue;
            }

            CSSDeclaration* nd = xs_realloc(declarations,
                sizeof(CSSDeclaration) * (size_t)(decl_count + 1));
            if (!nd) { free(trimmed_property); free(trimmed_value); continue; }
            declarations = nd;
            declarations[decl_count].property = trimmed_property;
            declarations[decl_count].value = trimmed_value;
            decl_count++;
        }
        if (*p == '}') p++; // skip '}'

        // Append rule.
        CSSRule* nr = xs_realloc(sheet->rules,
            sizeof(CSSRule) * (size_t)(sheet->rule_count + 1));
        if (!nr) {
            /* Drop this rule's data on OOM; keep what we have. */
            free(trimmed_selector);
            for (int j = 0; j < decl_count; j++) {
                free(declarations[j].property);
                free(declarations[j].value);
            }
            free(declarations);
            break;
        }
        sheet->rules = nr;
        sheet->rules[sheet->rule_count].selector = trimmed_selector;
        sheet->rules[sheet->rule_count].declarations = declarations;
        sheet->rules[sheet->rule_count].declaration_count = decl_count;
        sheet->rule_count++;
    }
    return sheet;
}

/* Parse a bare declaration block (no selector/braces), e.g. the value of an
   inline style="" attribute. Returns a stylesheet with a single rule whose
   selector is "" (FEAT-013). */
CSSStyleSheet* parse_css_declarations(const char* decls) {
    if (!decls) return NULL;
    /* Wrap in a synthetic rule and reuse the main parser. */
    size_t n = strlen(decls);
    char* wrapped = xs_malloc(n + 4);
    if (!wrapped) return NULL;
    wrapped[0] = '{';
    memcpy(wrapped + 1, decls, n);
    wrapped[n + 1] = '}';
    wrapped[n + 2] = '\0';
    CSSStyleSheet* s = parse_css(wrapped);
    free(wrapped);
    return s;
}

void free_stylesheet(CSSStyleSheet* sheet) {
    if (!sheet) return;
    for (int i = 0; i < sheet->rule_count; i++) {
        free(sheet->rules[i].selector);
        for (int j = 0; j < sheet->rules[i].declaration_count; j++) {
            free(sheet->rules[i].declarations[j].property);
            free(sheet->rules[i].declarations[j].value);
        }
        free(sheet->rules[i].declarations);
    }
    free(sheet->rules);
    free(sheet);
}

// --- CSS Application ---

// Attach computed style structure to the DOM node if it does not already have one.
void ensure_computed_style(DOMNode* node) {
    if (!node) return;
    if (!node->style) {
        node->style = xs_malloc(sizeof(ComputedStyle));
        if (!node->style) return;
        node->style->width = NULL;
        node->style->height = NULL;
        node->style->background = NULL;
        node->style->text_align = NULL;
        node->style->font_size = NULL;
        node->style->color = NULL;
        node->style->margin = NULL;
        node->style->padding = NULL;
    }
}

static void set_style_prop(DOMNode* node, const char* prop, const char* value) {
    ensure_computed_style(node);
    if (!node->style) return;
    char** slot = NULL;
    if (strcasecmp(prop, "width") == 0)            slot = &node->style->width;
    else if (strcasecmp(prop, "height") == 0)      slot = &node->style->height;
    else if (strcasecmp(prop, "background") == 0)  slot = &node->style->background;
    else if (strcasecmp(prop, "background-color") == 0) slot = &node->style->background;
    else if (strcasecmp(prop, "font-size") == 0)   slot = &node->style->font_size;
    else if (strcasecmp(prop, "text-align") == 0)  slot = &node->style->text_align;
    else if (strcasecmp(prop, "color") == 0)       slot = &node->style->color;
    else if (strcasecmp(prop, "margin") == 0)      slot = &node->style->margin;
    else if (strcasecmp(prop, "padding") == 0)     slot = &node->style->padding;
    if (!slot) return;
    char* dup = xs_strdup(value);
    if (!dup) return;
    if (*slot) free(*slot);
    *slot = dup;
}

/* A single matched declaration with its cascade priority. */
typedef struct {
    const char* property;
    const char* value;
    int         specificity;
    int         order;       /* source order (later wins on ties) */
} MatchedDecl;

/* Pre-parsed selector lists, one per rule (parallel to sheet->rules). */
typedef struct {
    SelectorList** lists;    /* lists[i] for rule i */
    int            count;
} CompiledSheet;

static CompiledSheet* compile_sheet(CSSStyleSheet* sheet) {
    CompiledSheet* cs = xs_malloc(sizeof *cs);
    if (!cs) return NULL;
    cs->count = sheet->rule_count;
    cs->lists = xs_calloc((size_t)sheet->rule_count, sizeof(SelectorList*));
    if (!cs->lists && sheet->rule_count > 0) { free(cs); return NULL; }
    for (int i = 0; i < sheet->rule_count; i++)
        cs->lists[i] = selector_list_parse(sheet->rules[i].selector);
    return cs;
}

static void free_compiled(CompiledSheet* cs) {
    if (!cs) return;
    for (int i = 0; i < cs->count; i++)
        selector_list_free(cs->lists[i]);
    free(cs->lists);
    free(cs);
}

/* Apply matched declarations in cascade order (specificity asc, then source
   order asc) so the highest-priority value is written last. */
static int cmp_matched(const void* a, const void* b) {
    const MatchedDecl* x = a;
    const MatchedDecl* y = b;
    if (x->specificity != y->specificity) return x->specificity - y->specificity;
    return x->order - y->order;
}

static void cascade_node(CompiledSheet* cs, CSSStyleSheet* sheet, DOMNode* node) {
    if (!node || !node->name || node->name[0] == '#') return;

    MatchedDecl* matched = NULL;
    size_t mcount = 0, mcap = 0;
    int order = 0;

    for (int r = 0; r < sheet->rule_count; r++) {
        SelectorList* sl = cs->lists[r];
        if (!sl) continue;
        int best_spec = -1;
        for (int s = 0; s < sl->count; s++) {
            if (sl->items[s].valid && selector_matches(&sl->items[s], node)) {
                if (sl->items[s].specificity > best_spec)
                    best_spec = sl->items[s].specificity;
            }
        }
        if (best_spec < 0) { order += sheet->rules[r].declaration_count; continue; }
        for (int d = 0; d < sheet->rules[r].declaration_count; d++) {
            if (!xs_grow((void**)&matched, &mcap, mcount + 1, sizeof(MatchedDecl))) break;
            matched[mcount].property = sheet->rules[r].declarations[d].property;
            matched[mcount].value    = sheet->rules[r].declarations[d].value;
            matched[mcount].specificity = best_spec;
            matched[mcount].order    = order++;
            mcount++;
        }
    }

    if (mcount > 1)
        qsort(matched, mcount, sizeof(MatchedDecl), cmp_matched);
    for (size_t i = 0; i < mcount; i++)
        set_style_prop(node, matched[i].property, matched[i].value);
    free(matched);

    /* Inline style="" wins over author rules (FEAT-013): apply last. */
    const char* inline_style = dom_attr(node, "style");
    if (inline_style && *inline_style) {
        CSSStyleSheet* tmp = parse_css_declarations(inline_style);
        if (tmp) {
            for (int d = 0; d < tmp->rule_count; d++)
                for (int j = 0; j < tmp->rules[d].declaration_count; j++)
                    set_style_prop(node, tmp->rules[d].declarations[j].property,
                                   tmp->rules[d].declarations[j].value);
            free_stylesheet(tmp);
        }
    }
}

static void apply_rules(CompiledSheet* cs, CSSStyleSheet* sheet, DOMNode* node) {
    if (!node) return;
    cascade_node(cs, sheet, node);
    for (int i = 0; i < node->children_count; i++)
        apply_rules(cs, sheet, node->children[i]);
}

void apply_stylesheet_to_dom(CSSStyleSheet* sheet, DOMNode* dom) {
    if (!sheet || !dom) return;
    CompiledSheet* cs = compile_sheet(sheet);
    if (!cs) return;
    apply_rules(cs, sheet, dom);
    free_compiled(cs);
}

/* Inheritance pass (FEAT-005): run ONCE after all stylesheets have cascaded.
   Propagates inherited properties (currently `color`) from each element to its
   descendants when they did not set their own value. */
static void inherit_node(DOMNode* node, const char* inherited_color) {
    if (!node) return;
    const char* my_color = inherited_color;
    if (node->style && node->style->color) {
        my_color = node->style->color;        /* own value wins, propagates */
    } else if (my_color) {
        set_style_prop(node, "color", my_color);  /* inherit (elements + #text) */
    }
    for (int i = 0; i < node->children_count; i++)
        inherit_node(node->children[i], my_color);
}

void style_inherit(DOMNode* dom) {
    inherit_node(dom, NULL);
}
