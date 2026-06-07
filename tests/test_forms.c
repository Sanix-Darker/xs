#include "test_util.h"
#include "../forms.h"
#include "../parser.h"
#include <string.h>

TEST(form_encode_basic) {
    char out[128];
    form_url_encode("hello world", out, sizeof out);
    CHECK_STR_EQ("hello+world", out);
}

TEST(form_encode_special) {
    char out[128];
    form_url_encode("a&b=c/d", out, sizeof out);
    CHECK_STR_EQ("a%26b%3Dc%2Fd", out);
}

TEST(form_encode_unreserved) {
    char out[128];
    form_url_encode("Az0-_.~", out, sizeof out);
    CHECK_STR_EQ("Az0-_.~", out);
}

TEST(form_query_build) {
    const char* names[]  = { "q", "lang" };
    const char* values[] = { "hello world", "en" };
    char out[256];
    form_build_query(names, values, 2, out, sizeof out);
    CHECK_STR_EQ("q=hello+world&lang=en", out);
}

TEST(form_owner_walk) {
    DOMNode *root = parse_html(
        "<form action=\"/s\"><div><input name=\"q\" value=\"x\"></div></form>");
    /* find the input */
    DOMNode *stack[128]; int sp = 0; stack[sp++] = root;
    DOMNode *input = NULL, *form = NULL;
    while (sp) {
        DOMNode *n = stack[--sp];
        if (n->name && strcmp(n->name, "input") == 0) input = n;
        if (n->name && strcmp(n->name, "form") == 0) form = n;
        for (int i = 0; i < n->children_count; i++) stack[sp++] = n->children[i];
    }
    CHECK_NOT_NULL(input);
    CHECK_NOT_NULL(form);
    if (input) CHECK(form_owner(input) == form);
    free_dom(root);
}

TEST(form_collect) {
    DOMNode *root = parse_html(
        "<form><input name=\"a\" value=\"1\">"
        "<input name=\"b\" value=\"2\">"
        "<input type=\"submit\" name=\"go\" value=\"Go\"></form>");
    DOMNode *form = NULL;
    DOMNode *stack[128]; int sp = 0; stack[sp++] = root;
    while (sp) { DOMNode *n = stack[--sp];
        if (n->name && strcmp(n->name, "form") == 0) { form = n; break; }
        for (int i = 0; i < n->children_count; i++) stack[sp++] = n->children[i]; }
    CHECK_NOT_NULL(form);
    const char* names[8]; const char* values[8];
    int n = form_collect_fields(form, names, values, 8);
    CHECK_EQ_INT(2, n);   /* submit excluded */
    char q[256];
    form_build_query(names, values, n, q, sizeof q);
    CHECK_STR_EQ("a=1&b=2", q);
    free_dom(root);
}
