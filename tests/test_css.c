#include "test_util.h"
#include "../css.h"
#include <string.h>

static const char *decl_value(CSSRule *rule, const char *prop) {
    for (int i = 0; i < rule->declaration_count; i++)
        if (strcmp(rule->declarations[i].property, prop) == 0)
            return rule->declarations[i].value;
    return NULL;
}

static CSSRule *rule_for(CSSStyleSheet *s, const char *sel) {
    for (int i = 0; i < s->rule_count; i++)
        if (strcmp(s->rules[i].selector, sel) == 0)
            return &s->rules[i];
    return NULL;
}

TEST(css_basic_rule) {
    CSSStyleSheet *s = parse_css("p { color: red; font-size: 18px; }");
    CHECK_NOT_NULL(s);
    CHECK_EQ_INT(1, s->rule_count);
    CSSRule *r = rule_for(s, "p");
    CHECK_NOT_NULL(r);
    if (r) {
        CHECK_STR_EQ("red", decl_value(r, "color"));
        CHECK_STR_EQ("18px", decl_value(r, "font-size"));
    }
    free_stylesheet(s);
}

/* FIX-006: a comment containing braces/semicolons must not desync parsing. */
TEST(css_comment_with_braces) {
    CSSStyleSheet *s = parse_css("/* a { b } ; c */ p { color: red; }");
    CHECK_NOT_NULL(s);
    CHECK_EQ_INT(1, s->rule_count);
    CSSRule *r = rule_for(s, "p");
    CHECK_NOT_NULL(r);
    if (r) CHECK_STR_EQ("red", decl_value(r, "color"));
    free_stylesheet(s);
}

TEST(css_comment_inside_value) {
    CSSStyleSheet *s = parse_css("p { color: red /* x */ ; }");
    CSSRule *r = rule_for(s, "p");
    CHECK_NOT_NULL(r);
    if (r) CHECK_STR_EQ("red", decl_value(r, "color"));
    free_stylesheet(s);
}

TEST(css_comment_in_selector) {
    CSSStyleSheet *s = parse_css("h1 /* big */ { font-size: 32px; }");
    /* selector should be just "h1" after comment strip + trim */
    CSSRule *r = rule_for(s, "h1");
    CHECK_NOT_NULL(r);
    if (r) CHECK_STR_EQ("32px", decl_value(r, "font-size"));
    free_stylesheet(s);
}

TEST(css_unterminated_comment) {
    /* Must not crash; yields the color decl before the comment. */
    CSSStyleSheet *s = parse_css("p { color: red; } /* oops no close");
    CHECK_NOT_NULL(s);
    CSSRule *r = rule_for(s, "p");
    CHECK_NOT_NULL(r);
    if (r) CHECK_STR_EQ("red", decl_value(r, "color"));
    free_stylesheet(s);
}

TEST(css_multiple_rules) {
    CSSStyleSheet *s = parse_css(
        "h1 { font-size: 32px; }\n"
        "p { font-size: 18px; }\n"
        ".note { background: #ffffcc; }\n");
    CHECK_EQ_INT(3, s->rule_count);
    CHECK_NOT_NULL(rule_for(s, "h1"));
    CHECK_NOT_NULL(rule_for(s, "p"));
    CHECK_NOT_NULL(rule_for(s, ".note"));
    free_stylesheet(s);
}

TEST(css_empty_and_null) {
    CHECK(parse_css(NULL) == NULL);
    CSSStyleSheet *s = parse_css("");
    CHECK_NOT_NULL(s);
    if (s) CHECK_EQ_INT(0, s->rule_count);
    free_stylesheet(s);
}

TEST(css_missing_semicolon_last_decl) {
    CSSStyleSheet *s = parse_css("p { color: red }");
    CSSRule *r = rule_for(s, "p");
    CHECK_NOT_NULL(r);
    if (r) CHECK_STR_EQ("red", decl_value(r, "color"));
    free_stylesheet(s);
}

/* FEAT-005: color inheritance from an ancestor to descendant text. */
TEST(css_color_inheritance) {
    DOMNode *root = parse_html("<body><p>hi <em>there</em></p></body>");
    CSSStyleSheet *s = parse_css("body { color: red; }");
    apply_stylesheet_to_dom(s, root);
    style_inherit(root);
    free_stylesheet(s);

    /* find the em element */
    DOMNode *stack[128]; int sp = 0; stack[sp++] = root;
    DOMNode *em = NULL;
    while (sp) {
        DOMNode *n = stack[--sp];
        if (n->name && strcmp(n->name, "em") == 0) { em = n; break; }
        for (int i = 0; i < n->children_count; i++) stack[sp++] = n->children[i];
    }
    CHECK_NOT_NULL(em);
    if (em) {
        CHECK_NOT_NULL(em->style);
        if (em->style) CHECK_STR_EQ("red", em->style->color);
    }
    free_dom(root);
}

/* FEAT-005: a child's explicit color overrides the inherited value, and that
   override does not leak back to siblings. */
TEST(css_color_override) {
    DOMNode *root = parse_html("<body><p id=\"a\">x</p><p>y</p></body>");
    CSSStyleSheet *s = parse_css("body{color:red} #a{color:blue}");
    apply_stylesheet_to_dom(s, root);
    style_inherit(root);
    free_stylesheet(s);

    DOMNode *body = NULL, *pa = NULL, *pb = NULL;
    DOMNode *stack[128]; int sp = 0; stack[sp++] = root;
    while (sp) {
        DOMNode *n = stack[--sp];
        if (n->name && strcmp(n->name, "body") == 0) body = n;
        for (int i = 0; i < n->children_count; i++) stack[sp++] = n->children[i];
    }
    CHECK_NOT_NULL(body);
    if (body) {
        for (int i = 0; i < body->children_count; i++) {
            DOMNode *c = body->children[i];
            if (c->name && strcmp(c->name, "p") == 0) {
                const char *id = dom_id(c);
                if (id && strcmp(id, "a") == 0) pa = c; else pb = c;
            }
        }
    }
    CHECK_NOT_NULL(pa);
    CHECK_NOT_NULL(pb);
    if (pa && pa->style) CHECK_STR_EQ("blue", pa->style->color);
    if (pb && pb->style) CHECK_STR_EQ("red", pb->style->color);  /* inherited, not blue */
    free_dom(root);
}
