#include "layout.h"
#include "css.h"
#include "style.h"
#include "table.h"
#include "tag_tables.h"
#include "tagid.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>

/* ------------------------------------------------------------------ */
/* Kindle-like tunables                                               */
enum {
    PAGE_MARGIN_X      = 30,
    PARAGRAPH_SPACING  = 16,
    HEADING_MARGIN_TOP = 24,
    HEADING_MARGIN_BOT = 12,
    LIST_INDENT        = 25,
    BLOCK_SPACING      = 10,
    INLINE_GAP         = 4,
    BLOCKQUOTE_INDENT  = 30,

    FONT_H1   = 28,
    FONT_H2   = 24,
    FONT_H3   = 20,
    FONT_H4   = 18,
    FONT_H5   = 16,
    FONT_H6   = 15,
    FONT_BODY = 16,
    FONT_CODE = 14,
    FONT_SMALL = 13
};

/* Layout context threaded through recursion */
typedef struct {
    int base_x, avail_w, cur_y, cur_inline_x;
    int font_size, is_bold, is_italic;
    int in_list;       /* 0=none, 1=ul, 2=ol */
    int list_counter;
    int align;         /* ALIGN_* inherited text alignment */
    int in_pre;        /* 1 inside <pre>/<textarea>: preserve whitespace */
    size_t line_start; /* index of first box on the current line (for align) */
    const char *href;
} LayoutContext;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                   */

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

/* Text zoom (MISS-009). */
static int g_zoom = 100;
void layout_set_zoom(int percent) {
    if (percent < 50) percent = 50;
    if (percent > 300) percent = 300;
    g_zoom = percent;
}
int layout_get_zoom(void) { return g_zoom; }
static inline int zoom_px(int px) { return px * g_zoom / 100; }

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

static inline bool has_visible_text(const char *txt)
{
    for (const unsigned char *p = (const unsigned char*)txt; p && *p; ++p)
        if (!isspace(*p)) return true;
    return false;
}

/* ------------------------------------------------------------------ */
/* Measure text width via the FontProvider at the *actual* size/style    */
/* (FIX-008). With no provider, use a deterministic approximation so      */
/* headless --dump output is stable across machines.                      */

static int approx_text_width(const char *text, int px)
{
    /* Nominal average advance ~0.4375em (7px at 16px); scales with size. */
    return text ? (int)strlen(text) * 7 * px / FONT_BODY : 0;
}

static int measure_text_width(const FontProvider *fonts, const char *text,
                              int px, int bold, int italic)
{
    if (!text || !*text) return 0;
    if (fonts && fonts->measure) {
        int w = fonts->measure(fonts->ctx, text, px, bold, italic);
        if (w >= 0) return w;
    }
    return approx_text_width(text, px);
}

/* ------------------------------------------------------------------ */
/* Heading level from tag name (0 if not a heading) */

static int heading_level(const char *tag)
{
    if (!tag || (tag[0] != 'h' && tag[0] != 'H')) return 0;
    if (tag[1] >= '1' && tag[1] <= '6' && tag[2] == '\0')
        return tag[1] - '0';
    return 0;
}

static int heading_font_size(int level)
{
    switch (level) {
    case 1: return FONT_H1;
    case 2: return FONT_H2;
    case 3: return FONT_H3;
    case 4: return FONT_H4;
    case 5: return FONT_H5;
    case 6: return FONT_H6;
    default: return FONT_BODY;
    }
}

/* Check if tag is a structural block that gets a wireframe border */
static bool is_structural(const char *tag)
{
    if (!tag) return false;
    return icmp(tag, "div") == 0 || icmp(tag, "section") == 0 ||
           icmp(tag, "article") == 0 || icmp(tag, "nav") == 0 ||
           icmp(tag, "header") == 0 || icmp(tag, "footer") == 0 ||
           icmp(tag, "main") == 0 || icmp(tag, "aside") == 0 ||
           icmp(tag, "table") == 0 || icmp(tag, "form") == 0;
}

/* Get CSS font-size if set on this node, resolved to px (FIX-007). em is
   resolved against the inherited font size. Returns 0 if not set. */
static int css_font_size(DOMNode *node, int parent_font_px)
{
    if (!node || !node->style || !node->style->font_size)
        return 0;
    Length l = style_parse_length(node->style->font_size);
    if (l.unit == LEN_AUTO) return 0;
    int px = style_resolve_length(l, parent_font_px, parent_font_px, 0);
    return px > 0 ? px : 0;
}

/* Resolve a CSS width string against a containing width (FIX-007). Returns -1
   if not set / auto. */
static int css_width_px(DOMNode *node, int containing_px, int font_px)
{
    if (!node || !node->style || !node->style->width) return -1;
    Length l = style_parse_length(node->style->width);
    if (l.unit == LEN_AUTO) return -1;
    return style_resolve_length(l, containing_px, font_px, -1);
}

/* Text alignment for this node, or -1 if not set. */
static int css_text_align(DOMNode *node)
{
    if (!node || !node->style || !node->style->text_align) return -1;
    return style_parse_align(node->style->text_align);
}

/* Resolve CSS margin/padding top+bottom (px) for a block; -1 if unset. */
static void css_vertical_spacing(DOMNode *node, int font_px,
                                 int *mtop, int *mbot, int *ptop, int *pbot)
{
    *mtop = *mbot = *ptop = *pbot = -1;
    if (!node || !node->style) return;
    if (node->style->margin) {
        Edge e; style_parse_edge(node->style->margin, &e);
        *mtop = style_resolve_length(e.top, 0, font_px, 0);
        *mbot = style_resolve_length(e.bottom, 0, font_px, 0);
    }
    if (node->style->padding) {
        Edge e; style_parse_edge(node->style->padding, &e);
        *ptop = style_resolve_length(e.top, 0, font_px, 0);
        *pbot = style_resolve_length(e.bottom, 0, font_px, 0);
    }
}

/* ------------------------------------------------------------------ */
/* Push a box with hints into the layout */

static LayoutBox *push_box(Layout *lay, int x, int y, int w, int h,
                           DOMNode *node, const char *href, LayoutHints hints)
{
    if (!ensure_capacity(lay, 1)) return NULL;
    LayoutBox *b = &lay->boxes[lay->count++];
    b->x = x;
    b->y = y;
    b->width = w;
    b->height = h;
    b->node = node;
    b->href = (char*)href;
    b->hints = hints;
    b->aligned = 0;
    b->run_off = 0;
    b->run_len = 0;
    return b;
}

/* Align text/inline boxes in [from, to) by grouping them into lines (same y)
   and shifting each line within [base_x, base_x+avail_w] per `align`
   (FEAT-002). Shifts are computed against each line's current left edge so the
   result is stable. */
static void align_block_lines(Layout *lay, size_t from, size_t to,
                              int base_x, int avail_w, int align)
{
    if (align != ALIGN_CENTER && align != ALIGN_RIGHT) return;
    size_t i = from;
    while (i < to) {
        int line_y = lay->boxes[i].y;
        size_t j = i;
        int min_x = INT_MAX, max_r = INT_MIN;
        while (j < to && lay->boxes[j].y == line_y) {
            LayoutBox *b = &lay->boxes[j];
            /* skip block/structural boxes (full width) when measuring */
            if (!b->hints.is_list_item && !b->hints.is_hr) {
                if (b->x < min_x) min_x = b->x;
                if (b->x + b->width > max_r) max_r = b->x + b->width;
            }
            j++;
        }
        if (min_x != INT_MAX && max_r != INT_MIN) {
            int used = max_r - min_x;
            int space = avail_w - used;
            if (space > 0) {
                int target_left = (align == ALIGN_CENTER)
                                ? base_x + space / 2
                                : base_x + space;          /* RIGHT */
                int delta = target_left - min_x;
                if (delta != 0) {
                    for (size_t k = i; k < j; k++) {
                        LayoutBox *b = &lay->boxes[k];
                        if (b->aligned) continue;          /* claimed by a descendant */
                        if (!b->hints.is_list_item && !b->hints.is_hr) {
                            b->x += delta;
                            b->aligned = 1;
                        }
                    }
                }
            }
        }
        i = j;
    }
}

/* ------------------------------------------------------------------ */
/* Recursive layout                                                   */

static void
layout_node(DOMNode *node, Layout *lay, const FontProvider *fonts,
            LayoutContext *ctx, int depth);

/* Measure a cell's preferred (single-line) and minimum (longest word) widths
   by summing/maxing its text descendants at the body font size. */
static void cell_measure(DOMNode *node, const FontProvider *fonts,
                         int font_px, int *pref, int *minw)
{
    if (!node) return;
    if (node->name && strcmp(node->name, "#text") == 0 && node->text) {
        /* preferred: whole text width; minimum: longest single word */
        int w = measure_text_width(fonts, node->text, font_px, 0, 0);
        *pref += w;
        /* longest word */
        const char *p = node->text;
        while (*p) {
            while (*p && xs_is_html_space((unsigned char)*p)) p++;
            const char *s = p;
            while (*p && !xs_is_html_space((unsigned char)*p)) p++;
            if (p > s) {
                char tmp[256];
                size_t len = (size_t)(p - s);
                if (len >= sizeof tmp) len = sizeof tmp - 1;
                memcpy(tmp, s, len); tmp[len] = '\0';
                int ww = measure_text_width(fonts, tmp, font_px, 0, 0);
                if (ww > *minw) *minw = ww;
            }
        }
        return;
    }
    for (int i = 0; i < node->children_count; i++)
        cell_measure(node->children[i], fonts, font_px, pref, minw);
}

/* Lay out a <table> as a simple grid (MISS-005). Returns the bottom y. */
static void layout_table(DOMNode *node, Layout *lay, const FontProvider *fonts,
                         LayoutContext *ctx, int depth)
{
    TableGrid *g = table_build_grid(node);
    if (!g || g->rows == 0 || g->cols == 0) {
        table_grid_free(g);
        return;
    }

    int cols = g->cols, rows = g->rows;
    int *pref = xs_calloc((size_t)cols, sizeof(int));
    int *minw = xs_calloc((size_t)cols, sizeof(int));
    int *cw   = xs_calloc((size_t)cols, sizeof(int));
    if (!pref || !minw || !cw) { free(pref); free(minw); free(cw); table_grid_free(g); return; }

    const int CELL_PAD = 6;
    int fs = ctx->font_size;

    /* Measure preferred/min widths per column. */
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            DOMNode *cell = g->cells[r * cols + c];
            if (!cell) continue;
            int p = 0, m = 0;
            cell_measure(cell, fonts, fs, &p, &m);
            p += 2 * CELL_PAD; m += 2 * CELL_PAD;
            if (p > pref[c]) pref[c] = p;
            if (m > minw[c]) minw[c] = m;
        }
    }

    int avail = ctx->avail_w;
    table_distribute_widths(pref, minw, cols, avail, cw);

    int line_h = fs * 14 / 10;
    int row_h = line_h + 2 * CELL_PAD;
    int top = ctx->cur_y;

    for (int r = 0; r < rows; r++) {
        int x = ctx->base_x;
        int max_cell_bottom = ctx->cur_y + row_h;
        for (int c = 0; c < cols; c++) {
            DOMNode *cell = g->cells[r * cols + c];
            int w = cw[c];
            /* cell border box */
            LayoutHints hints = {0};
            hints.font_size = fs;
            hints.show_border = 1;
            if (cell) {
                push_box(lay, x, ctx->cur_y, w, row_h, cell, NULL, hints);
                /* lay out cell content inside padding box */
                LayoutContext cc = *ctx;
                cc.base_x = x + CELL_PAD;
                cc.avail_w = w - 2 * CELL_PAD;
                cc.cur_y = ctx->cur_y + CELL_PAD;
                cc.cur_inline_x = cc.base_x;
                cc.align = ALIGN_LEFT;
                int is_th = cell->name && strcmp(cell->name, "th") == 0;
                cc.is_bold = is_th ? 1 : ctx->is_bold;
                for (int i = 0; i < cell->children_count; i++)
                    layout_node(cell->children[i], lay, fonts, &cc, depth + 1);
                if (cc.cur_inline_x != cc.base_x) cc.cur_y += line_h;
                int bottom = cc.cur_y + CELL_PAD;
                if (bottom > max_cell_bottom) max_cell_bottom = bottom;
            }
            x += w;
        }
        ctx->cur_y = max_cell_bottom;
    }

    /* outer table border */
    LayoutHints thints = {0};
    thints.font_size = fs;
    thints.show_border = 1;
    /* (table outline approximated by cell borders; skip explicit outer box) */
    (void)top; (void)thints;

    free(pref); free(minw); free(cw);
    table_grid_free(g);
}

static void
layout_node_impl(DOMNode *node, Layout *lay, const FontProvider *fonts,
            LayoutContext *ctx, int depth);

static void
layout_node(DOMNode *node, Layout *lay, const FontProvider *fonts,
            LayoutContext *ctx, int depth)
{
    if (node && node->name && icmp(node->name, "table") == 0) {
        /* Flush any pending inline content, then lay out the table grid. */
        if (ctx->cur_inline_x != ctx->base_x) {
            ctx->cur_y += ctx->font_size * 14 / 10;
            ctx->cur_inline_x = ctx->base_x;
        }
        ctx->cur_y += BLOCK_SPACING;
        layout_table(node, lay, fonts, ctx, depth);
        ctx->cur_y += BLOCK_SPACING;
        return;
    }
    layout_node_impl(node, lay, fonts, ctx, depth);
}

static void
layout_node_impl(DOMNode *node, Layout *lay, const FontProvider *fonts,
            LayoutContext *ctx, int depth)
{
    if (!node || !node->name) return;
    if (depth > XS_MAX_DEPTH) return;   /* FIX-015: bound layout recursion */

    /* Skip non-rendered elements (fast TagId check — FEAT-010/PERF). */
    switch (node->tag) {
    case TAG_SCRIPT: case TAG_STYLE: case TAG_HEAD:
    case TAG_META:   case TAG_LINK:  case TAG_TITLE:
        return;
    default: break;
    }

    /* Determine href: either from this node (if <a>) or inherited */
    const char *href = ctx->href;
    if (node->href) href = node->href;

    /* Check for CSS font-size override (resolved against inherited size) */
    int css_fs = css_font_size(node, ctx->font_size);

    /* ---- <br> special: force line break ---- */
    if (icmp(node->name, "br") == 0) {
        int line_h = ctx->font_size * 14 / 10;
        ctx->cur_y += line_h;
        ctx->cur_inline_x = ctx->base_x;
        return;
    }

    /* ---- <hr> special: horizontal rule ---- */
    if (icmp(node->name, "hr") == 0) {
        ctx->cur_inline_x = ctx->base_x;
        ctx->cur_y += BLOCK_SPACING;
        LayoutHints hints = {0};
        hints.is_hr = 1;
        hints.font_size = ctx->font_size;
        push_box(lay, ctx->base_x, ctx->cur_y,
                 ctx->avail_w, 4, node, NULL, hints);
        ctx->cur_y += 4 + BLOCK_SPACING;
        return;
    }

    /* -----------------------------  BLOCK  ------------------------- */
    if (IS_BLOCK(node->name))
    {
        /* Flush inline cursor */
        if (ctx->cur_inline_x != ctx->base_x) {
            int line_h = ctx->font_size * 14 / 10;
            ctx->cur_y += line_h;
            ctx->cur_inline_x = ctx->base_x;
        }

        int hlevel = heading_level(node->name);

        /* Save context for child layout */
        LayoutContext child = *ctx;

        /* Determine font size for this block (zoom-aware — MISS-009) */
        if (css_fs > 0)
            child.font_size = zoom_px(css_fs);
        else if (hlevel)
            child.font_size = zoom_px(heading_font_size(hlevel));
        else if (icmp(node->name, "pre") == 0)
            child.font_size = zoom_px(FONT_CODE);

        if (icmp(node->name, "pre") == 0 || icmp(node->name, "textarea") == 0)
            child.in_pre = 1;

        /* Extra spacing before headings */
        if (hlevel)
            ctx->cur_y += HEADING_MARGIN_TOP;

        /* Paragraph spacing */
        if (icmp(node->name, "p") == 0)
            ctx->cur_y += PARAGRAPH_SPACING / 2;

        /* CSS margin-top / padding-top (FEAT-006): when specified, add on top
           of default spacing. margin is outside the box; padding is inside. */
        int css_mtop, css_mbot, css_ptop, css_pbot;
        css_vertical_spacing(node, child.font_size, &css_mtop, &css_mbot, &css_ptop, &css_pbot);
        if (css_mtop >= 0) ctx->cur_y += css_mtop;

        /* List handling */
        if (icmp(node->name, "ul") == 0) {
            child.in_list = 1;
            child.list_counter = 0;
            child.base_x = ctx->base_x + LIST_INDENT;
            child.avail_w = ctx->avail_w - LIST_INDENT;
            child.cur_inline_x = child.base_x;
        } else if (icmp(node->name, "ol") == 0) {
            child.in_list = 2;
            child.list_counter = 0;
            child.base_x = ctx->base_x + LIST_INDENT;
            child.avail_w = ctx->avail_w - LIST_INDENT;
            child.cur_inline_x = child.base_x;
        }

        /* Blockquote indent */
        if (icmp(node->name, "blockquote") == 0) {
            child.base_x = ctx->base_x + BLOCKQUOTE_INDENT;
            child.avail_w = ctx->avail_w - BLOCKQUOTE_INDENT;
            child.cur_inline_x = child.base_x;
        }

        /* <li> handling: push marker box, increment counter */
        if (icmp(node->name, "li") == 0) {
            if (ctx->in_list == 2)
                ctx->list_counter++;
            child.list_counter = ctx->list_counter;

            LayoutHints marker_hints = {0};
            marker_hints.font_size = child.font_size;
            marker_hints.is_bold = child.is_bold;
            marker_hints.is_list_item = 1;
            marker_hints.list_index = (ctx->in_list == 2) ? ctx->list_counter : 0;
            marker_hints.is_link = (href != NULL);

            int line_h = child.font_size * 14 / 10;
            push_box(lay, ctx->base_x - LIST_INDENT, ctx->cur_y,
                     LIST_INDENT, line_h, node, (char*)href, marker_hints);
        }

        /* Width from CSS if set (typed, resolves %, em, px — FIX-007) */
        int block_w = child.avail_w;
        {
            int css_w = css_width_px(node, ctx->avail_w, child.font_size);
            if (css_w > 0 && css_w < block_w) block_w = css_w;
        }

        /* Text alignment: this block's value, else inherited (FEAT-002). */
        int my_align = css_text_align(node);
        if (my_align >= 0) child.align = my_align;

        /* Push box at cur_y with height=0 (will fixup later) */
        int start_y = ctx->cur_y;
        size_t box_idx = lay->count;

        LayoutHints block_hints = {0};
        block_hints.font_size = child.font_size;
        block_hints.is_heading = hlevel;
        block_hints.is_bold = child.is_bold || (hlevel > 0);
        block_hints.is_italic = child.is_italic;
        block_hints.is_link = (href != NULL);
        block_hints.show_border = is_structural(node->name) ? 1 : 0;
        block_hints.is_pre = (icmp(node->name, "pre") == 0);
        block_hints.is_blockquote = (icmp(node->name, "blockquote") == 0);

        push_box(lay, ctx->base_x, start_y, block_w, 0,
                 node, (char*)href, block_hints);

        /* Headings are bold */
        if (hlevel) child.is_bold = 1;

        /* Lay out children */
        child.cur_y = start_y + (css_ptop >= 0 ? css_ptop : 0);  /* padding-top */
        child.cur_inline_x = child.base_x;
        child.href = href;
        size_t content_start = lay->count;  /* first child box (for alignment) */

        for (int i = 0; i < node->children_count; ++i)
            layout_node(node->children[i], lay, fonts, &child, depth + 1);

        /* Flush trailing inline content */
        if (child.cur_inline_x != child.base_x) {
            int line_h = child.font_size * 14 / 10;
            child.cur_y += line_h;
        }

        /* Apply text alignment to the lines produced by this block (FEAT-002).
           Only when this block establishes a non-left alignment; nested blocks
           handle their own content. */
        if ((child.align == ALIGN_CENTER || child.align == ALIGN_RIGHT) &&
            content_start < lay->count) {
            align_block_lines(lay, content_start, lay->count,
                              child.base_x, child.avail_w, child.align);
        }

        /* Fixup: set the actual height of the block box */
        if (css_pbot >= 0) child.cur_y += css_pbot;   /* padding-bottom inside box */
        int actual_h = child.cur_y - start_y;
        if (actual_h < 0) actual_h = 0;

        /* Minimum height for empty blocks */
        if (actual_h == 0 && node->children_count == 0) {
            int line_h = child.font_size * 14 / 10;
            actual_h = line_h;
        }

        if (box_idx < lay->count)
            lay->boxes[box_idx].height = actual_h;

        ctx->cur_y = start_y + actual_h;

        /* CSS margin-bottom (outside the box). */
        if (css_mbot >= 0) ctx->cur_y += css_mbot;

        /* Extra spacing after headings */
        if (hlevel)
            ctx->cur_y += HEADING_MARGIN_BOT;

        /* Paragraph spacing */
        if (icmp(node->name, "p") == 0)
            ctx->cur_y += PARAGRAPH_SPACING / 2;

        /* Block spacing */
        ctx->cur_y += BLOCK_SPACING;

        ctx->cur_inline_x = ctx->base_x;

        /* Propagate list counter back */
        if (icmp(node->name, "li") == 0)
            ctx->list_counter = child.list_counter;

        return;
    }

    /* -----------------------------  INLINE ------------------------- */
    if (IS_INLINE(node->name))
    {
        /* ---- <img>: inline-block box sized by width/height attrs (MISS-002).
           Decoding is build-gated on SDL2_image; layout reserves the box and
           render shows the image or an alt-text placeholder. ---- */
        if (icmp(node->name, "img") == 0) {
            int iw = 0, ih = 0;
            const char *wa = dom_attr(node, "width");
            const char *ha = dom_attr(node, "height");
            if (wa) iw = style_resolve_length(style_parse_length(wa), ctx->avail_w, ctx->font_size, 0);
            if (ha) ih = style_resolve_length(style_parse_length(ha), ctx->avail_w, ctx->font_size, 0);
            if (iw <= 0) iw = 120;   /* default placeholder size */
            if (ih <= 0) ih = 80;
            if (iw > ctx->avail_w) iw = ctx->avail_w;

            int line_h = ctx->font_size * 14 / 10;
            if (ctx->cur_inline_x + iw > ctx->base_x + ctx->avail_w &&
                ctx->cur_inline_x != ctx->base_x) {
                ctx->cur_y += line_h;
                ctx->cur_inline_x = ctx->base_x;
            }
            LayoutHints hints = {0};
            hints.font_size = ctx->font_size;
            hints.is_image = 1;
            hints.is_link = (href != NULL);
            push_box(lay, ctx->cur_inline_x, ctx->cur_y, iw, ih, node, (char*)href, hints);
            ctx->cur_inline_x += iw + INLINE_GAP;
            /* grow line height if the image is taller than text */
            if (ih > line_h) {
                /* reserve vertical space: advance baseline tracking */
            }
            return;
        }

        /* #text nodes produce one box PER WORD, breaking on html-space at
           layout time (FEAT-011). The DOM keeps whole text nodes — no per-word
           shredding — so node count stays O(structure) and script/style text
           is never mutated. Each word box is a substring view into node->text. */
        if (strcmp(node->name, "#text") == 0) {
            if (!has_visible_text(node->text)) return;

            int fs = ctx->font_size;
            if (css_fs > 0) fs = zoom_px(css_fs);
            int line_h = fs * 14 / 10;

            /* <pre>/<textarea>: preserve whitespace — emit the whole text as a
               single box (no word breaking) so spaces/tabs are kept. */
            if (ctx->in_pre) {
                int width = measure_text_width(fonts, node->text, fs,
                                               ctx->is_bold, ctx->is_italic);
                LayoutHints hints = {0};
                hints.font_size = fs;
                hints.is_bold = ctx->is_bold;
                hints.is_italic = ctx->is_italic;
                hints.is_link = (href != NULL);
                push_box(lay, ctx->cur_inline_x, ctx->cur_y,
                         width, line_h, node, (char*)href, hints);
                ctx->cur_inline_x += width + INLINE_GAP;
                return;
            }

            const char *text = node->text;
            const char *p = text;
            while (*p) {
                /* skip leading html-space */
                while (*p && xs_is_html_space((unsigned char)*p)) p++;
                if (!*p) break;
                const char *start = p;
                while (*p && !xs_is_html_space((unsigned char)*p)) p++;
                int off = (int)(start - text);
                int len = (int)(p - start);

                /* measure this word (copy into a bounded buffer) */
                char wbuf[256];
                int clen = len < (int)sizeof(wbuf) - 1 ? len : (int)sizeof(wbuf) - 1;
                memcpy(wbuf, start, (size_t)clen);
                wbuf[clen] = '\0';
                int width = measure_text_width(fonts, wbuf, fs,
                                               ctx->is_bold, ctx->is_italic);

                /* wrap if necessary */
                if (ctx->cur_inline_x + width > ctx->base_x + ctx->avail_w &&
                    ctx->cur_inline_x != ctx->base_x) {
                    ctx->cur_y       += line_h;
                    ctx->cur_inline_x = ctx->base_x;
                }

                LayoutHints hints = {0};
                hints.font_size = fs;
                hints.is_bold = ctx->is_bold;
                hints.is_italic = ctx->is_italic;
                hints.is_link = (href != NULL);

                LayoutBox *b = push_box(lay, ctx->cur_inline_x, ctx->cur_y,
                                        width, line_h, node, (char*)href, hints);
                if (b) { b->run_off = off; b->run_len = len; }

                ctx->cur_inline_x += width + INLINE_GAP;
            }
            return;
        }

        /* Inline wrappers: <b>, <strong>, <em>, <i>, <a>, <code>, <small>, etc. */
        LayoutContext child = *ctx;
        child.href = href;

        if (icmp(node->name, "b") == 0 || icmp(node->name, "strong") == 0)
            child.is_bold = 1;
        if (icmp(node->name, "em") == 0 || icmp(node->name, "i") == 0)
            child.is_italic = 1;
        if (icmp(node->name, "code") == 0 && css_fs == 0)
            child.font_size = zoom_px(FONT_CODE);
        if (icmp(node->name, "small") == 0 && css_fs == 0)
            child.font_size = zoom_px(FONT_SMALL);
        if (css_fs > 0)
            child.font_size = zoom_px(css_fs);

        for (int i = 0; i < node->children_count; ++i)
            layout_node(node->children[i], lay, fonts, &child, depth + 1);

        /* Propagate cursor position back */
        ctx->cur_y = child.cur_y;
        ctx->cur_inline_x = child.cur_inline_x;
        return;
    }

    /* -----------------------------  OTHER / UNKNOWN ---------------- */
    LayoutContext child = *ctx;
    child.href = href;
    if (css_fs > 0) child.font_size = zoom_px(css_fs);

    for (int i = 0; i < node->children_count; ++i)
        layout_node(node->children[i], lay, fonts, &child, depth + 1);

    ctx->cur_y = child.cur_y;
    ctx->cur_inline_x = child.cur_inline_x;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */

Layout *layout_dom(DOMNode *root, const FontProvider *fonts, int window_w)
{
    Layout *lay = calloc(1, sizeof *lay);
    if (!lay) return NULL;

    lay->dom = root;

    LayoutContext ctx = {0};
    ctx.base_x = PAGE_MARGIN_X;
    ctx.avail_w = (window_w > 0 ? window_w : 800) - 2 * PAGE_MARGIN_X;
    ctx.cur_y = 10;
    ctx.cur_inline_x = ctx.base_x;
    ctx.font_size = zoom_px(FONT_BODY);

    layout_node(root, lay, fonts, &ctx, 0);
    return lay;
}

void free_layout(Layout *lay)
{
    if (!lay) return;
    free(lay->boxes);
    /* FIX-009: the Layout does NOT own the DOM. The caller (Document/render)
       owns and frees the DOM separately. This removes the fragile detach dance. */
    free(lay);
}

/* ------------------------------------------------------------------ */
/* Layout dump (FEAT-003 / testing/dump-format.md). Stable text format */
/* used by golden tests. Coordinates are raw layout space (pre-scroll). */

static void dump_escape(FILE *out, const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; p && *p; ++p) {
        switch (*p) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:   fputc(*p, out);     break;
        }
    }
}

void dump_layout(const Layout *lay, FILE *out, int window_w, int window_h)
{
    fprintf(out, "XS-DUMP 1 width=%d height=%d\n", window_w, window_h);
    if (!lay) return;

    for (size_t i = 0; i < lay->count; ++i) {
        const LayoutBox *b = &lay->boxes[i];
        const LayoutHints *h = &b->hints;
        char flags[11];
        flags[0] = h->is_bold      ? 'B' : '-';
        flags[1] = h->is_italic    ? 'I' : '-';
        flags[2] = h->is_heading   ? 'H' : '-';
        flags[3] = h->is_link      ? 'L' : '-';
        flags[4] = h->is_list_item ? 'M' : '-';
        flags[5] = h->is_hr        ? 'R' : '-';
        flags[6] = h->is_pre       ? 'P' : '-';
        flags[7] = h->is_blockquote? 'Q' : '-';
        flags[8] = h->show_border  ? 'S' : '-';
        flags[9] = h->is_image     ? 'G' : '-';
        flags[10] = '\0';

        const char *tag = (b->node && b->node->name) ? b->node->name : "?";
        fprintf(out, "BOX x=%d y=%d w=%d h=%d fs=%d flags=%s tag=%s",
                b->x, b->y, b->width, b->height, h->font_size, flags, tag);

        if (b->node && b->node->name &&
            strcmp(b->node->name, "#text") == 0 && b->node->text) {
            fputs(" text=\"", out);
            if (b->run_len > 0) {
                /* substring view (FEAT-011) */
                char tmp[512];
                int n = b->run_len < (int)sizeof(tmp) - 1 ? b->run_len : (int)sizeof(tmp) - 1;
                memcpy(tmp, b->node->text + b->run_off, (size_t)n);
                tmp[n] = '\0';
                dump_escape(out, tmp);
            } else {
                dump_escape(out, b->node->text);
            }
            fputc('"', out);
        }
        fputc('\n', out);
    }
}
