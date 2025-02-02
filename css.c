#include "css.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- Utility Functions ---

// Trim whitespace from start and end of a string. Returns a new allocated string.
static char* trim(const char* str) {
    while (isspace(*str)) str++;
    int len = strlen(str);
    while (len > 0 && isspace(str[len-1])) len--;
    char* result = malloc(len + 1);
    strncpy(result, str, len);
    result[len] = '\0';
    return result;
}

// --- CSS Parsing ---

// This is a very naive parser. It assumes that the CSS is well-formed and uses the format:
// selector { property: value; property: value; }
CSSStyleSheet* parse_css(const char* css_text) {
    CSSStyleSheet* sheet = malloc(sizeof(CSSStyleSheet));
    sheet->rules = NULL;
    sheet->rule_count = 0;

    const char* p = css_text;
    while (*p) {
        // Skip whitespace and comments if desired.
        while (*p && isspace(*p)) p++;

        // Read selector until '{'
        const char* sel_start = p;
        while (*p && *p != '{') p++;
        if (*p != '{') break; // end if no '{'
        int sel_len = p - sel_start;
        char* selector = malloc(sel_len + 1);
        strncpy(selector, sel_start, sel_len);
        selector[sel_len] = '\0';
        char* trimmed_selector = trim(selector);
        free(selector);

        p++; // skip '{'
        // Parse declarations until '}'
        CSSDeclaration* declarations = NULL;
        int decl_count = 0;
        while (*p && *p != '}') {
            // Skip whitespace.
            while (*p && isspace(*p)) p++;
            // Read property name until ':'
            const char* prop_start = p;
            while (*p && *p != ':') p++;
            if (*p != ':') break;
            int prop_len = p - prop_start;
            char* property = malloc(prop_len + 1);
            strncpy(property, prop_start, prop_len);
            property[prop_len] = '\0';
            char* trimmed_property = trim(property);
            free(property);
            p++; // skip ':'
            // Read value until ';'
            const char* val_start = p;
            while (*p && *p != ';') p++;
            int val_len = p - val_start;
            char* value = malloc(val_len + 1);
            strncpy(value, val_start, val_len);
            value[val_len] = '\0';
            char* trimmed_value = trim(value);
            free(value);
            // Skip ';'
            if (*p == ';') p++;

            // Append declaration
            declarations = realloc(declarations, sizeof(CSSDeclaration) * (decl_count + 1));
            declarations[decl_count].property = trimmed_property;
            declarations[decl_count].value = trimmed_value;
            decl_count++;
        }
        if (*p == '}') p++; // skip '}'

        // Append rule.
        sheet->rules = realloc(sheet->rules, sizeof(CSSRule) * (sheet->rule_count + 1));
        sheet->rules[sheet->rule_count].selector = trimmed_selector;
        sheet->rules[sheet->rule_count].declarations = declarations;
        sheet->rules[sheet->rule_count].declaration_count = decl_count;
        sheet->rule_count++;
    }
    return sheet;
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

// A very basic matching function: supports matching by tag name (or "#text") and by class if the DOM node
// has a "class" attribute (if you extended your DOM). For now, we assume the selector is a tag name.
static int matches_selector(DOMNode* node, const char* selector) {
    if (!node || !node->name) return 0;
    // For now, case-insensitive string comparison for tag names.
    return (strcasecmp(node->name, selector) == 0);
}

// Attach computed style structure to the DOM node if it does not already have one.
void ensure_computed_style(DOMNode* node) {
    // Assume node->style is NULL if not computed.
    if (!node) return;
    if (!node->style) {
        node->style = malloc(sizeof(ComputedStyle));
        node->style->width = NULL;
        node->style->height = NULL;
        node->style->background = NULL;
    }
}

static void apply_rule_to_node(CSSRule* rule, DOMNode* node) {
    if (!matches_selector(node, rule->selector))
        return;

    ensure_computed_style(node);

    for (int i = 0; i < rule->declaration_count; i++) {
        CSSDeclaration* decl = &rule->declarations[i];
        if (strcasecmp(decl->property, "width") == 0) {
            if (node->style->width) free(node->style->width);
            node->style->width = strdup(decl->value);
        } else if (strcasecmp(decl->property, "height") == 0) {
            if (node->style->height) free(node->style->height);
            node->style->height = strdup(decl->value);
        } else if (strcasecmp(decl->property, "background") == 0) {
            if (node->style->background) free(node->style->background);
            node->style->background = strdup(decl->value);
        }
        // TODO: extend for more options on positions
    }
}

// Recursively apply the stylesheet rules to a DOM tree.
static void apply_rules(CSSStyleSheet* sheet, DOMNode* node) {
    if (!node) return;
    for (int i = 0; i < sheet->rule_count; i++) {
        apply_rule_to_node(&sheet->rules[i], node);
    }
    for (int i = 0; i < node->children_count; i++) {
        apply_rules(sheet, node->children[i]);
    }
}

void apply_stylesheet_to_dom(CSSStyleSheet* sheet, DOMNode* dom) {
    apply_rules(sheet, dom);
}
