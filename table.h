#ifndef XS_TABLE_H
#define XS_TABLE_H

#include "parser.h"
#include <stddef.h>

/*
 * Simple table grid model (MISS-005). No colspan/rowspan in this phase: each
 * <td>/<th> occupies one cell. Rows come from <tr> anywhere under the table
 * (thead/tbody/tfoot are transparent).
 */

typedef struct {
    DOMNode** cells;   /* row-major: cells[r*cols + c], may be NULL */
    int rows;
    int cols;
} TableGrid;

/* Build a grid from a <table> element. Caller frees with table_grid_free.
   Returns a grid with rows/cols possibly 0 if the table is empty. */
TableGrid* table_build_grid(DOMNode* table);
void       table_grid_free(TableGrid* g);

/* Distribute `avail` width across `n` columns given each column's preferred
   and minimum widths. Writes resolved widths into out[] (sum <= avail when
   possible; each >= its min). Pure + testable. */
void table_distribute_widths(const int* pref, const int* min, int n,
                             int avail, int* out);

#endif /* XS_TABLE_H */
