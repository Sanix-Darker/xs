#include "layout.h"
#include "css.h"
#include "tag_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>

/* ------------------------------------------------------------------ */
/* Tunables                                                           */
enum {
    BLOCK_INDENT       = 20,
    VERTICAL_MARGIN    = 10,
    INLINE_MARGIN      = 5,
    DEFAULT_WINDOW_W   = 800,
    BASE_MARGIN        = 20,
    INLINE_HEIGHT_DEF  = 20,
    BLOCK_HEIGHT_DEF   = 30
};

/* ------------------------------------------------------------------ */
/* Internal helpers                                                   */

/* fast case‑insensitive strcmp that assumes ASCII letters            */
static inline int icmp(const char *a, const char *b)
{
    for ( ; *a && *b; ++a, ++b) {
        unsigned ca = (unsigned char)*a, cb = (unsigned char)*b;
        ca = (ca >= 'A' && ca <= 'Z') ? ca + 32 : ca;
        cb = (cb >= 'A' && cb <= 'Z') ? cb + 32 : cb;
        if (ca != cb) return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* binary search in the tag tables (tables are sorted, lower‑case)    */
static bool tag_in_table(const char *tag, const char *const table[], size_t n)
{
    if (!tag) return false;
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        int cmp = icmp(tag, table[mid]);
        if (cmp == 0) return true;
        (cmp < 0) ? (hi = mid) : (lo = mid + 1);
    }
    return false;
}
#define IS_BLOCK(t)  tag_in_table((t), block_tags,  BLOCK_TAGS_N)
#define IS_INLINE(t) tag_in_table((t), inline_tags, INLINE_TAGS_N)

/* strtol wrapper that understands "<num>px", clamps to ≥0,            *
 * and falls back to default_value on error/overflow                   */
static int parse_dimension(const char *s, int default_value)
{
    if (!s || !*s) return default_value;
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || v < 0 || v > INT_MAX) return default_value;
    return (int)v;
}

/* Layout vector utility                                              */
static int ensure_capacity(Layout *lay, size_t extra)
{
    if (lay->count + extra <= lay->capacity) return 1;
    size_t newcap = lay->capacity ? lay->capacity : 16;
    while (newcap < lay->count + extra) newcap <<= 1;
    void *tmp = realloc(lay->boxes, newcap * sizeof *lay->boxes);
    if (!tmp) return 0;
    lay->boxes = tmp;
    lay->capacity = newcap;
    return 1;
}

/* whitespace test for text nodes                                     */
static inline bool has_visible_text(const char *txt)
{
    for (const unsigned char *p = (const unsigned char*)txt; p && *p; ++p)
        if (!isspace(*p)) return true;
    return false;
}

/* ------------------------------------------------------------------ */

static void
layout_node(DOMNode *node, Layout *lay,
            int base_x, int *cur_y, int *cur_inline_x, int avail_w)
{
    if (!node || !node->name) return;

    if (icmp(node->name, "script") == 0 || icmp(node->name, "style") == 0)
        return;

    /* -----------------------------  BLOCK  ------------------------- */
    if (IS_BLOCK(node->name))
    {
        *cur_inline_x = base_x;               /* flush inline          */

        int width  = node->style && node->style->width
                       ? parse_dimension(node->style->width, avail_w)
                       : avail_w;

        int height = node->style && node->style->height
                       ? parse_dimension(node->style->height, BLOCK_HEIGHT_DEF)
                       : BLOCK_HEIGHT_DEF;

        /* headings get a bit more height                              */
        if      (icmp(node->name, "h1") == 0) height = 40;
        else if (icmp(node->name, "h2") == 0) height = 35;
        else if (icmp(node->name, "h3") == 0) height = 30;

        if (!ensure_capacity(lay, 1)) return; /* OOM guard             */
        lay->boxes[lay->count++] = (LayoutBox){ .x=base_x, .y=*cur_y,
                                                .width=width, .height=height,
                                                .node=node };

        *cur_y += height + VERTICAL_MARGIN;

        int child_base_x = base_x + BLOCK_INDENT;
        int child_avail  = avail_w - BLOCK_INDENT;
        int child_inline_x = child_base_x;

        for (int i = 0; i < node->children_count; ++i)
            layout_node(node->children[i], lay,
                        child_base_x, cur_y, &child_inline_x, child_avail);

        *cur_inline_x = base_x;               /* reset inline cursor   */
        return;
    }

    /* -----------------------------  INLINE ------------------------- */
    if (IS_INLINE(node->name))
    {
        int width  = 0;
        int height = INLINE_HEIGHT_DEF;

        if (strcmp(node->name, "#text") == 0) {
            if (!has_visible_text(node->text)) return;
            width = (int)strlen(node->text) * 7;   /* cheap approx.    */
        } else {
            width = 50;                            /* generic default  */
            if (node->style && node->style->width)
                width = parse_dimension(node->style->width, width);
        }
        if (node->style && node->style->height)
            height = parse_dimension(node->style->height, height);

        /* wrap line if necessary                                       */
        if (*cur_inline_x + width > base_x + avail_w) {
            *cur_y        += height + VERTICAL_MARGIN;
            *cur_inline_x  = base_x;
        }

        if (!ensure_capacity(lay, 1)) return;       /* OOM guard        */
        lay->boxes[lay->count++] = (LayoutBox){ .x=*cur_inline_x, .y=*cur_y,
                                                .width=width, .height=height,
                                                .node=node };

        *cur_inline_x += width + INLINE_MARGIN;

        /* recurse into children, inheriting current inline position    */
        for (int i = 0; i < node->children_count; ++i)
            layout_node(node->children[i], lay,
                        *cur_inline_x, cur_y, cur_inline_x,
                        base_x + avail_w - (*cur_inline_x - base_x));
        return;
    }

    /* -----------------------------  OTHER / UNKNOWN ---------------- */
    for (int i = 0; i < node->children_count; ++i)
        layout_node(node->children[i], lay,
                    base_x, cur_y, cur_inline_x, avail_w);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */

Layout *layout_dom(DOMNode *root)
{
    Layout *lay = calloc(1, sizeof *lay);
    if (!lay) return NULL;

    lay->dom = root;

    int cur_y  = 10;                    /* top margin                 */
    int base_x = BASE_MARGIN;
    int cur_inline_x = base_x;
    int avail_w = DEFAULT_WINDOW_W - 2 * BASE_MARGIN;

    layout_node(root, lay, base_x, &cur_y, &cur_inline_x, avail_w);
    return lay;
}

void free_layout(Layout *lay)
{
    if (!lay) return;
    free(lay->boxes);
    if (lay->dom)
        free_dom(lay->dom);
    free(lay);
}
