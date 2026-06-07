#include "test_util.h"
#include "../parser.h"
#include "../util.h"
#include "../tagid.h"
#include <string.h>

/* Find first descendant element with the given (lowercased) tag name. */
static DOMNode *find_tag(DOMNode *n, const char *name) {
    if (!n) return NULL;
    if (n->name && strcmp(n->name, name) == 0) return n;
    for (int i = 0; i < n->children_count; i++) {
        DOMNode *r = find_tag(n->children[i], name);
        if (r) return r;
    }
    return NULL;
}

/* Concatenate all #text descendants into buf (bounded). */
static void gather_text(DOMNode *n, char *buf, size_t cap) {
    if (!n) return;
    if (n->name && strcmp(n->name, "#text") == 0 && n->text) {
        size_t used = strlen(buf);
        size_t add = strlen(n->text);
        if (used + add + 1 < cap) { memcpy(buf + used, n->text, add); buf[used+add] = '\0'; }
    }
    for (int i = 0; i < n->children_count; i++)
        gather_text(n->children[i], buf, cap);
}

/* FIX-001: unknown/custom/svg tags must have exact, lowercased names with no
   trailing garbage from the non-NUL-terminated original_tag string piece. */
TEST(parser_unknown_tags_bounded) {
    DOMNode *root = parse_html("<svg><circle></circle></svg><x-foo>hi</x-foo>");
    CHECK_NOT_NULL(root);
    DOMNode *svg = find_tag(root, "svg");
    CHECK_NOT_NULL(svg);
    if (svg) CHECK_STR_EQ("svg", svg->name);
    DOMNode *circle = find_tag(root, "circle");
    CHECK_NOT_NULL(circle);
    DOMNode *xfoo = find_tag(root, "x-foo");
    CHECK_NOT_NULL(xfoo);
    if (xfoo) CHECK_STR_EQ("x-foo", xfoo->name);  /* exact length, no garbage */
    free_dom(root);
}

TEST(parser_tag_lowercased) {
    DOMNode *root = parse_html("<DIV><P>Hi</P></DIV>");
    CHECK_NOT_NULL(find_tag(root, "div"));
    CHECK_NOT_NULL(find_tag(root, "p"));
    free_dom(root);
}

TEST(parser_href_captured) {
    DOMNode *root = parse_html("<a href=\"https://example.com/\">link</a>");
    DOMNode *a = find_tag(root, "a");
    CHECK_NOT_NULL(a);
    if (a) CHECK_STR_EQ("https://example.com/", a->href);
    free_dom(root);
}

/* FIX-002: <script> text must survive intact (not split into words) so JS and
   CSS source are not mangled. */
TEST(parser_script_text_intact) {
    DOMNode *root = parse_html("<script>var a = 1 + 2; if (a > 2) a = a + 1;</script>");
    split_text_nodes(root);  /* must NOT touch script content */
    DOMNode *script = find_tag(root, "script");
    CHECK_NOT_NULL(script);
    char buf[256] = {0};
    gather_text(script, buf, sizeof buf);
    CHECK_STR_EQ("var a = 1 + 2; if (a > 2) a = a + 1;", buf);
    free_dom(root);
}

TEST(parser_style_text_intact) {
    DOMNode *root = parse_html("<style>div { color: red; }</style>");
    char *css = extract_style_text(root);
    CHECK_NOT_NULL(css);
    /* extract adds a trailing newline; check the content prefix. */
    if (css) CHECK_STRN_EQ("div { color: red; }", css, 19);
    free(css);
    free_dom(root);
}

/* FIX-003: words split on any html-space; tabs/newlines are delimiters, not
   part of words, and produce no empty tokens. */
TEST(parser_whitespace_split) {
    DOMNode *root = parse_html("<p>  the\tquick\nbrown   fox </p>");
    split_text_nodes(root);
    DOMNode *p = find_tag(root, "p");
    CHECK_NOT_NULL(p);
    /* Count #text children that are non-empty words. */
    int words = 0;
    char first[32] = {0}, last[32] = {0};
    for (int i = 0; p && i < p->children_count; i++) {
        DOMNode *c = p->children[i];
        if (c->name && strcmp(c->name, "#text") == 0 && c->text && c->text[0]) {
            if (words == 0) snprintf(first, sizeof first, "%s", c->text);
            snprintf(last, sizeof last, "%s", c->text);
            words++;
            /* no word should contain a tab/newline */
            CHECK(strchr(c->text, '\t') == NULL);
            CHECK(strchr(c->text, '\n') == NULL);
        }
    }
    CHECK_EQ_INT(4, words);
    CHECK_STR_EQ("the", first);
    CHECK_STR_EQ("fox", last);
    free_dom(root);
}

/* FIX-015: deep nesting must not crash and must respect the depth cap. */
static int tree_depth(DOMNode *n) {
    int max = 0;
    for (int i = 0; i < n->children_count; i++) {
        int d = tree_depth(n->children[i]);
        if (d > max) max = d;
    }
    return max + 1;
}

TEST(parser_deep_nesting_capped) {
    /* Build 5000 nested <div>s. */
    size_t n = 5000;
    char *html = malloc(n * 5 + n * 6 + 1);
    CHECK_NOT_NULL(html);
    if (!html) return;
    size_t off = 0;
    for (size_t i = 0; i < n; i++) { memcpy(html + off, "<div>", 5); off += 5; }
    for (size_t i = 0; i < n; i++) { memcpy(html + off, "</div>", 6); off += 6; }
    html[off] = '\0';

    DOMNode *root = parse_html(html);  /* must not stack-overflow */
    CHECK_NOT_NULL(root);
    if (root) {
        int d = tree_depth(root);
        CHECK(d <= XS_MAX_DEPTH + 4);  /* bounded (small slack for wrappers) */
    }
    free_dom(root);
    free(html);
}

TEST(parser_null_input) {
    CHECK(parse_html(NULL) == NULL);  /* graceful, no crash */
}

/* MISS-001: generic attribute capture + class/id helpers. */
TEST(parser_attributes_captured) {
    DOMNode *root = parse_html(
        "<a href=\"x\" class=\"c1 c2\" id=\"i\" data-y=\"z\">link</a>");
    DOMNode *a = find_tag(root, "a");
    CHECK_NOT_NULL(a);
    if (a) {
        CHECK_STR_EQ("x", dom_attr(a, "href"));
        CHECK_STR_EQ("i", dom_id(a));
        CHECK_STR_EQ("z", dom_attr(a, "data-y"));
        CHECK(dom_has_class(a, "c1"));
        CHECK(dom_has_class(a, "c2"));
        CHECK_EQ_INT(0, dom_has_class(a, "c3"));
        CHECK_EQ_INT(0, dom_has_class(a, "c"));   /* not a prefix match */
        CHECK(dom_attr(a, "missing") == NULL);
    }
    free_dom(root);
}

TEST(parser_attr_case_insensitive) {
    DOMNode *root = parse_html("<div DATA-X=\"1\" Class=\"big\"></div>");
    DOMNode *d = find_tag(root, "div");
    CHECK_NOT_NULL(d);
    if (d) {
        CHECK_STR_EQ("1", dom_attr(d, "data-x"));   /* lookup is case-insensitive */
        CHECK_STR_EQ("1", dom_attr(d, "DATA-X"));
        CHECK(dom_has_class(d, "big"));
    }
    free_dom(root);
}

TEST(parser_parent_pointers) {
    DOMNode *root = parse_html("<div><p><a href=\"x\">hi</a></p></div>");
    DOMNode *a = find_tag(root, "a");
    CHECK_NOT_NULL(a);
    if (a) {
        CHECK_NOT_NULL(a->parent);
        if (a->parent) CHECK_STR_EQ("p", a->parent->name);
        DOMNode *p = a->parent;
        CHECK_NOT_NULL(p->parent);
        if (p->parent) CHECK_STR_EQ("div", p->parent->name);
    }
    /* root has no parent */
    CHECK(root->parent == NULL);
    free_dom(root);
}

/* FEAT-010: interned tag ids set on nodes. */
TEST(parser_tag_ids) {
    DOMNode *root = parse_html("<div><p>hi</p><x-unknown></x-unknown></div>");
    DOMNode *div = find_tag(root, "div");
    DOMNode *p = find_tag(root, "p");
    DOMNode *unk = find_tag(root, "x-unknown");
    CHECK_NOT_NULL(div);
    CHECK_NOT_NULL(p);
    if (div) CHECK_EQ_INT(TAG_DIV, div->tag);
    if (p)   CHECK_EQ_INT(TAG_P, p->tag);
    if (unk) CHECK_EQ_INT(TAG_UNKNOWN, unk->tag);
    /* text node */
    DOMNode *t = find_tag(p, "#text");
    if (t) CHECK_EQ_INT(TAG_TEXT, t->tag);
    free_dom(root);
}
