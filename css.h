#ifndef CSS_H
#define CSS_H

#include "parser.h" // For DOMNode definition

// A CSS declaration, e.g. "width: 600px"
typedef struct {
    char* property;
    char* value;
} CSSDeclaration;

// A CSS rule: e.g. "div { width: 600px; height: 30px; }"
typedef struct {
    char* selector;             // e.g. "div", ".classname", "#id"
    CSSDeclaration* declarations; // Array of declarations.
    int declaration_count;
} CSSRule;

// A stylesheet is an array of CSS rules.
typedef struct {
    CSSRule* rules;
    int rule_count;
} CSSStyleSheet;

// Parse a CSS string and return a stylesheet.
CSSStyleSheet* parse_css(const char* css_text);

// Free the stylesheet.
void free_stylesheet(CSSStyleSheet* sheet);

// For simplicity, add computed style fields to your DOMNode structure.
// In production, you might want a separate structure.
// For this example, we assume a simple approach: we add a pointer for a style
// (here we just store a string for each property, but you could use more advanced types).
typedef struct ComputedStyle ComputedStyle; // forward declaration

// Attach computed style to a DOM node.
void apply_stylesheet_to_dom(CSSStyleSheet* sheet, DOMNode* dom);

// A helper: if a DOM node has no computed style, allocate one.
void ensure_computed_style(DOMNode* node);

#endif
