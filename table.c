#include "table.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static int is_row(const char* name) {
    return name && strcmp(name, "tr") == 0;
}
static int is_cell(const char* name) {
    return name && (strcmp(name, "td") == 0 || strcmp(name, "th") == 0);
}
static int is_row_group(const char* name) {
    return name && (strcmp(name, "thead") == 0 || strcmp(name, "tbody") == 0 ||
                    strcmp(name, "tfoot") == 0);
}

/* Collect <tr> rows, descending through row groups. */
static void collect_rows(DOMNode* node, DOMNode*** rows, int* count, size_t* cap) {
    for (int i = 0; i < node->children_count; i++) {
        DOMNode* c = node->children[i];
        if (!c->name) continue;
        if (is_row(c->name)) {
            if (!xs_grow((void**)rows, cap, (size_t)(*count + 1), sizeof(DOMNode*)))
                return;
            (*rows)[(*count)++] = c;
        } else if (is_row_group(c->name)) {
            collect_rows(c, rows, count, cap);
        }
    }
}

TableGrid* table_build_grid(DOMNode* table) {
    TableGrid* g = xs_calloc(1, sizeof *g);
    if (!g) return NULL;
    if (!table) return g;

    DOMNode** rows = NULL; int nrows = 0; size_t rcap = 0;
    collect_rows(table, &rows, &nrows, &rcap);

    /* column count = max cells in any row */
    int cols = 0;
    for (int r = 0; r < nrows; r++) {
        int n = 0;
        for (int i = 0; i < rows[r]->children_count; i++)
            if (is_cell(rows[r]->children[i]->name)) n++;
        if (n > cols) cols = n;
    }

    g->rows = nrows;
    g->cols = cols;
    if (nrows > 0 && cols > 0) {
        g->cells = xs_calloc((size_t)nrows * cols, sizeof(DOMNode*));
        if (!g->cells) { free(rows); g->rows = g->cols = 0; return g; }
        for (int r = 0; r < nrows; r++) {
            int ci = 0;
            for (int i = 0; i < rows[r]->children_count && ci < cols; i++) {
                if (is_cell(rows[r]->children[i]->name))
                    g->cells[r * cols + ci++] = rows[r]->children[i];
            }
        }
    }
    free(rows);
    return g;
}

void table_grid_free(TableGrid* g) {
    if (!g) return;
    free(g->cells);
    free(g);
}

void table_distribute_widths(const int* pref, const int* min, int n,
                             int avail, int* out) {
    if (n <= 0) return;
    long total_pref = 0, total_min = 0;
    for (int i = 0; i < n; i++) { total_pref += pref[i]; total_min += min[i]; }

    if (total_pref <= avail) {
        /* Everyone gets their preferred width; distribute slack evenly. */
        int slack = avail - (int)total_pref;
        int per = n ? slack / n : 0;
        for (int i = 0; i < n; i++) out[i] = pref[i] + per;
        /* give remainder to the last column */
        if (n > 0) out[n-1] += slack - per * n;
        return;
    }

    if (total_min >= avail) {
        /* Not enough room even for minimums; use minimums (may overflow). */
        for (int i = 0; i < n; i++) out[i] = min[i];
        return;
    }

    /* Between min and pref: distribute the available "extra" above minimum in
       proportion to each column's (pref - min). */
    long extra_room = avail - total_min;          /* > 0 */
    long total_flex = total_pref - total_min;     /* > 0 */
    int assigned = 0;
    for (int i = 0; i < n; i++) {
        long flex = pref[i] - min[i];
        int add = (total_flex > 0) ? (int)(extra_room * flex / total_flex) : 0;
        out[i] = min[i] + add;
        assigned += out[i];
    }
    /* rounding remainder to last column */
    if (n > 0) out[n-1] += (avail - assigned);
}
