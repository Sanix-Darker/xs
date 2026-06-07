#include "test_util.h"
#include "../table.h"
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

TEST(table_grid_dimensions) {
    DOMNode *root = parse_html(
        "<table><tr><th>A</th><th>B</th><th>C</th></tr>"
        "<tr><td>1</td><td>2</td><td>3</td></tr></table>");
    DOMNode *t = find_tag(root, "table");
    CHECK_NOT_NULL(t);
    TableGrid *g = table_build_grid(t);
    CHECK_NOT_NULL(g);
    if (g) {
        CHECK_EQ_INT(2, g->rows);
        CHECK_EQ_INT(3, g->cols);
        /* cell [0][0] is the A header */
        DOMNode *a = g->cells[0];
        CHECK_NOT_NULL(a);
        if (a) CHECK_STR_EQ("th", a->name);
    }
    table_grid_free(g);
    free_dom(root);
}

TEST(table_grid_ragged_rows) {
    DOMNode *root = parse_html(
        "<table><tr><td>1</td><td>2</td></tr>"
        "<tr><td>a</td><td>b</td><td>c</td></tr></table>");
    DOMNode *t = find_tag(root, "table");
    TableGrid *g = table_build_grid(t);
    CHECK_EQ_INT(2, g->rows);
    CHECK_EQ_INT(3, g->cols);   /* max cells per row */
    /* row 0 col 2 is missing -> NULL */
    CHECK(g->cells[0 * 3 + 2] == NULL);
    table_grid_free(g);
    free_dom(root);
}

TEST(table_widths_fit) {
    int pref[3] = {100, 50, 50};
    int minw[3] = {20, 20, 20};
    int out[3] = {0};
    table_distribute_widths(pref, minw, 3, 300, out);
    int sum = out[0] + out[1] + out[2];
    CHECK_EQ_INT(300, sum);        /* uses full available */
    CHECK(out[0] >= 100);          /* preferred respected, gets slack */
}

TEST(table_widths_squeeze) {
    int pref[3] = {200, 200, 200};  /* total 600 > avail */
    int minw[3] = {30, 30, 30};
    int out[3] = {0};
    table_distribute_widths(pref, minw, 3, 300, out);
    int sum = out[0] + out[1] + out[2];
    CHECK_EQ_INT(300, sum);
    for (int i = 0; i < 3; i++) CHECK(out[i] >= 30);  /* >= min */
}

TEST(table_widths_below_min) {
    int pref[2] = {100, 100};
    int minw[2] = {80, 80};   /* total min 160 > avail 100 */
    int out[2] = {0};
    table_distribute_widths(pref, minw, 2, 100, out);
    CHECK_EQ_INT(80, out[0]);  /* falls back to minimums */
    CHECK_EQ_INT(80, out[1]);
}

TEST(table_empty) {
    DOMNode *root = parse_html("<table></table>");
    DOMNode *t = find_tag(root, "table");
    TableGrid *g = table_build_grid(t);
    CHECK_NOT_NULL(g);
    if (g) { CHECK_EQ_INT(0, g->rows); CHECK_EQ_INT(0, g->cols); }
    table_grid_free(g);
    free_dom(root);
}
