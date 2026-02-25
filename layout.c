#include "layout.h"
#include "css.h"
#include "tag_tables.h"
#include <SDL2/SDL_ttf.h>
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

static int parse_dimension(const char *s, int default_value)
{
    if (!s || !*s) return default_value;
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || v < 0 || v > INT_MAX) return default_value;
    return (int)v;
}

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
/* Measure text width: use TTF_SizeUTF8 if font available, else approx */

static int measure_text_width(void *font, const char *text, int target_size)
{
    if (font && text && *text) {
        int w = 0;
        TTF_SizeUTF8((TTF_Font*)font, text, &w, NULL);
        /* Scale proportionally for different font sizes (base font is 16pt) */
        return w * target_size / FONT_BODY;
    }
    return text ? (int)strlen(text) * 7 * target_size / FONT_BODY : 0;
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

/* Get CSS font-size if set on this node */
static int css_font_size(DOMNode *node)
{
    if (!node || !node->style || !node->style->font_size)
        return 0;
    return parse_dimension(node->style->font_size, 0);
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
    return b;
}

/* ------------------------------------------------------------------ */
/* Recursive layout                                                   */

static void
layout_node(DOMNode *node, Layout *lay, void *font, LayoutContext *ctx)
{
    if (!node || !node->name) return;

    if (icmp(node->name, "script") == 0 || icmp(node->name, "style") == 0 ||
        icmp(node->name, "head") == 0 || icmp(node->name, "meta") == 0 ||
        icmp(node->name, "link") == 0 || icmp(node->name, "title") == 0)
        return;

    /* Determine href: either from this node (if <a>) or inherited */
    const char *href = ctx->href;
    if (node->href) href = node->href;

    /* Check for CSS font-size override */
    int css_fs = css_font_size(node);

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

        /* Determine font size for this block */
        if (css_fs > 0)
            child.font_size = css_fs;
        else if (hlevel)
            child.font_size = heading_font_size(hlevel);
        else if (icmp(node->name, "pre") == 0)
            child.font_size = FONT_CODE;

        /* Extra spacing before headings */
        if (hlevel)
            ctx->cur_y += HEADING_MARGIN_TOP;

        /* Paragraph spacing */
        if (icmp(node->name, "p") == 0)
            ctx->cur_y += PARAGRAPH_SPACING / 2;

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

        /* Width from CSS if set */
        int block_w = child.avail_w;
        if (node->style && node->style->width) {
            int css_w = parse_dimension(node->style->width, 0);
            if (css_w > 0 && css_w < block_w) block_w = css_w;
        }

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
        child.cur_y = start_y;
        child.cur_inline_x = child.base_x;
        child.href = href;

        for (int i = 0; i < node->children_count; ++i)
            layout_node(node->children[i], lay, font, &child);

        /* Flush trailing inline content */
        if (child.cur_inline_x != child.base_x) {
            int line_h = child.font_size * 14 / 10;
            child.cur_y += line_h;
        }

        /* Fixup: set the actual height of the block box */
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
        /* #text nodes produce a LayoutBox */
        if (strcmp(node->name, "#text") == 0) {
            if (!has_visible_text(node->text)) return;

            int fs = ctx->font_size;
            if (css_fs > 0) fs = css_fs;
            int width  = measure_text_width(font, node->text, fs);
            int line_h = fs * 14 / 10;

            /* wrap line if necessary */
            if (ctx->cur_inline_x + width > ctx->base_x + ctx->avail_w &&
                ctx->cur_inline_x != ctx->base_x) {
                ctx->cur_y        += line_h;
                ctx->cur_inline_x  = ctx->base_x;
            }

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

        /* Inline wrappers: <b>, <strong>, <em>, <i>, <a>, <code>, <small>, etc. */
        LayoutContext child = *ctx;
        child.href = href;

        if (icmp(node->name, "b") == 0 || icmp(node->name, "strong") == 0)
            child.is_bold = 1;
        if (icmp(node->name, "em") == 0 || icmp(node->name, "i") == 0)
            child.is_italic = 1;
        if (icmp(node->name, "code") == 0 && css_fs == 0)
            child.font_size = FONT_CODE;
        if (icmp(node->name, "small") == 0 && css_fs == 0)
            child.font_size = FONT_SMALL;
        if (css_fs > 0)
            child.font_size = css_fs;

        for (int i = 0; i < node->children_count; ++i)
            layout_node(node->children[i], lay, font, &child);

        /* Propagate cursor position back */
        ctx->cur_y = child.cur_y;
        ctx->cur_inline_x = child.cur_inline_x;
        return;
    }

    /* -----------------------------  OTHER / UNKNOWN ---------------- */
    LayoutContext child = *ctx;
    child.href = href;
    if (css_fs > 0) child.font_size = css_fs;

    for (int i = 0; i < node->children_count; ++i)
        layout_node(node->children[i], lay, font, &child);

    ctx->cur_y = child.cur_y;
    ctx->cur_inline_x = child.cur_inline_x;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */

Layout *layout_dom(DOMNode *root, void *font, int window_w)
{
    Layout *lay = calloc(1, sizeof *lay);
    if (!lay) return NULL;

    lay->dom = root;

    LayoutContext ctx = {0};
    ctx.base_x = PAGE_MARGIN_X;
    ctx.avail_w = (window_w > 0 ? window_w : 800) - 2 * PAGE_MARGIN_X;
    ctx.cur_y = 10;
    ctx.cur_inline_x = ctx.base_x;
    ctx.font_size = FONT_BODY;

    layout_node(root, lay, font, &ctx);
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
