#include "test_util.h"
#include "../select.h"
#include "../parser.h"
#include <string.h>

static DOMNode *find_tag(DOMNode *n, const char *name) {
    if (!n) return NULL;
    if (n->name && strcmp(n->name, name) == 0) return n;
    for (int i = 0; i < n->children_count; i++) {
        DOMNode *r = find_tag(n->children[i], name);
        if (r) return r;
    }
    return NULL;
}

static int matches_first(const char *sel_text, DOMNode *node) {
    SelectorList *l = selector_list_parse(sel_text);
    int m = 0;
    for (int i = 0; i < l->count; i++)
        if (selector_matches(&l->items[i], node)) { m = 1; break; }
    selector_list_free(l);
    return m;
}

TEST(select_type) {
    DOMNode *root = parse_html("<p>hi</p><div>x</div>");
    DOMNode *p = find_tag(root, "p");
    DOMNode *div = find_tag(root, "div");
    CHECK(matches_first("p", p));
    CHECK_EQ_INT(0, matches_first("p", div));
    free_dom(root);
}

TEST(select_class) {
    DOMNode *root = parse_html("<p class=\"a note b\">x</p>");
    DOMNode *p = find_tag(root, "p");
    CHECK(matches_first(".note", p));
    CHECK(matches_first("p.note", p));
    CHECK_EQ_INT(0, matches_first(".notes", p));   /* not a prefix */
    free_dom(root);
}

TEST(select_id) {
    DOMNode *root = parse_html("<div id=\"main\">x</div>");
    DOMNode *div = find_tag(root, "div");
    CHECK(matches_first("#main", div));
    CHECK(matches_first("div#main", div));
    CHECK_EQ_INT(0, matches_first("#other", div));
    free_dom(root);
}

TEST(select_universal) {
    DOMNode *root = parse_html("<p>x</p>");
    DOMNode *p = find_tag(root, "p");
    CHECK(matches_first("*", p));
    free_dom(root);
}

TEST(select_grouping) {
    DOMNode *root = parse_html("<h1>a</h1><h2>b</h2><p>c</p>");
    DOMNode *h1 = find_tag(root, "h1");
    DOMNode *h2 = find_tag(root, "h2");
    DOMNode *p = find_tag(root, "p");
    CHECK(matches_first("h1, h2", h1));
    CHECK(matches_first("h1, h2", h2));
    CHECK_EQ_INT(0, matches_first("h1, h2", p));
    free_dom(root);
}

TEST(select_descendant) {
    DOMNode *root = parse_html("<nav><span><a href=\"x\">l</a></span></nav><a href=\"y\">top</a>");
    /* there are two <a>; find the one inside nav */
    DOMNode *nav = find_tag(root, "nav");
    DOMNode *nav_a = find_tag(nav, "a");
    CHECK(matches_first("nav a", nav_a));
    /* The top-level <a> is the second one; locate it by walking */
    DOMNode *body = nav->parent;
    DOMNode *top_a = NULL;
    for (int i = 0; i < body->children_count; i++)
        if (body->children[i]->name && strcmp(body->children[i]->name, "a") == 0)
            top_a = body->children[i];
    CHECK_NOT_NULL(top_a);
    if (top_a) CHECK_EQ_INT(0, matches_first("nav a", top_a));
    free_dom(root);
}

TEST(select_specificity) {
    SelectorList *l = selector_list_parse("#id");
    SelectorList *l2 = selector_list_parse(".cls");
    SelectorList *l3 = selector_list_parse("div");
    CHECK(l->items[0].specificity > l2->items[0].specificity);
    CHECK(l2->items[0].specificity > l3->items[0].specificity);
    selector_list_free(l); selector_list_free(l2); selector_list_free(l3);
}

TEST(select_null_safe) {
    SelectorList *l = selector_list_parse(NULL);
    CHECK_NOT_NULL(l);
    if (l) CHECK_EQ_INT(0, l->count);
    selector_list_free(l);
}
