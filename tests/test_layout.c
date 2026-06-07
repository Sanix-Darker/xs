#include "test_util.h"
#include "../parser.h"
#include "../layout.h"
#include "../css.h"
#include <string.h>

/* Find first element with given tag via DFS. */
static DOMNode *dfs_tag(DOMNode *root, const char *tag) {
    DOMNode *stack[256]; int sp = 0; stack[sp++] = root;
    while (sp) {
        DOMNode *n = stack[--sp];
        if (n->name && strcmp(n->name, tag) == 0) return n;
        for (int i = 0; i < n->children_count && sp < 256; i++) stack[sp++] = n->children[i];
    }
    return NULL;
}

/* A measurement provider that returns a width depending NON-linearly on px so
   we can prove layout measures at the true size (FIX-008), not by scaling a
   single 16px measurement. width = strlen * px * px / 16  (quadratic in px). */
static int quad_measure(void *ctx, const char *text, int px, int bold, int italic) {
    (void)ctx; (void)bold; (void)italic;
    if (!text) return 0;
    return (int)strlen(text) * px * px / 16;
}

static LayoutBox *find_text_box(Layout *lay, const char *text) {
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->name && strcmp(b->node->name, "#text") == 0 &&
            b->node->text && strcmp(b->node->text, text) == 0)
            return b;
    }
    return NULL;
}

/* The provider must actually be used: a heading word measured at 28px should
   get the quadratic width, not a 16px-scaled width. */
TEST(layout_uses_true_size_measure) {
    DOMNode *root = parse_html("<h1>Word</h1><p>Word</p>");
    split_text_nodes(root);
    FontProvider fp = { NULL, quad_measure };
    Layout *lay = layout_dom(root, &fp, 800);
    CHECK_NOT_NULL(lay);

    LayoutBox *h1word = NULL, *pword = NULL;
    /* Both have text "Word"; the first occurrence is the h1 (28px), second is p (16px). */
    int seen = 0;
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->text && strcmp(b->node->text, "Word") == 0 &&
            strcmp(b->node->name, "#text") == 0) {
            if (seen == 0) h1word = b; else pword = b;
            seen++;
        }
    }
    CHECK_NOT_NULL(h1word);
    CHECK_NOT_NULL(pword);
    if (h1word && pword) {
        /* len("Word")=4. 28px -> 4*28*28/16 = 196. 16px -> 4*16 = 64. */
        CHECK_EQ_INT(196, h1word->width);
        CHECK_EQ_INT(64, pword->width);
    }
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

/* With no provider, the deterministic approximation is used (stable). */
TEST(layout_headless_approx_stable) {
    DOMNode *root = parse_html("<p>Hello</p>");
    split_text_nodes(root);
    Layout *lay = layout_dom(root, NULL, 800);
    LayoutBox *b = find_text_box(lay, "Hello");
    CHECK_NOT_NULL(b);
    /* approx = strlen*7*px/16 = 5*7*16/16 = 35 */
    if (b) CHECK_EQ_INT(35, b->width);
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

/* Wrapping: a long paragraph in a narrow viewport must produce multiple lines
   (boxes at increasing y). */
TEST(layout_wraps_long_text) {
    DOMNode *root = parse_html(
        "<p>one two three four five six seven eight nine ten eleven twelve</p>");
    split_text_nodes(root);
    Layout *lay = layout_dom(root, NULL, 200);  /* narrow */
    int distinct_y = 0, last_y = -99999;
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->name && strcmp(b->node->name, "#text") == 0) {
            if (b->y != last_y) { distinct_y++; last_y = b->y; }
        }
    }
    CHECK(distinct_y >= 2);  /* wrapped onto at least two lines */
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

/* MISS-002: <img> produces a box sized by width/height attributes. */
TEST(layout_img_sizing) {
    DOMNode *root = parse_html(
        "<img width=\"200\" height=\"120\" alt=\"x\">"
        "<img alt=\"noSize\">");
    split_text_nodes(root);
    Layout *lay = layout_dom(root, NULL, 800);
    int found_sized = 0, found_default = 0;
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->name && strcmp(b->node->name, "img") == 0) {
            CHECK(b->hints.is_image);
            if (b->width == 200 && b->height == 120) found_sized = 1;
            if (b->width == 120 && b->height == 80) found_default = 1;
        }
    }
    CHECK(found_sized);
    CHECK(found_default);
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

/* MISS-009: text zoom scales resolved font sizes proportionally. */
TEST(layout_text_zoom) {
    DOMNode *root = parse_html("<p>hello</p>");
    split_text_nodes(root);

    layout_set_zoom(100);
    Layout *l1 = layout_dom(root, NULL, 800);
    int fs1 = 0;
    for (size_t i = 0; i < l1->count; i++)
        if (l1->boxes[i].node && l1->boxes[i].node->text &&
            strcmp(l1->boxes[i].node->name, "#text") == 0) { fs1 = l1->boxes[i].hints.font_size; break; }
    l1->dom = NULL; free_layout(l1);

    layout_set_zoom(200);
    Layout *l2 = layout_dom(root, NULL, 800);
    int fs2 = 0;
    for (size_t i = 0; i < l2->count; i++)
        if (l2->boxes[i].node && l2->boxes[i].node->text &&
            strcmp(l2->boxes[i].node->name, "#text") == 0) { fs2 = l2->boxes[i].hints.font_size; break; }
    l2->dom = NULL; free_layout(l2);

    layout_set_zoom(100);   /* reset so other tests/goldens are unaffected */

    CHECK_EQ_INT(16, fs1);
    CHECK_EQ_INT(32, fs2);
    CHECK_EQ_INT(100, layout_get_zoom());
    free_dom(root);
}

TEST(layout_zoom_clamped) {
    layout_set_zoom(10000);
    CHECK(layout_get_zoom() <= 300);
    layout_set_zoom(1);
    CHECK(layout_get_zoom() >= 50);
    layout_set_zoom(100);
}

/* FEAT-011: layout breaks text into per-word boxes WITHOUT adding DOM nodes.
   A paragraph of N words yields N word boxes but the DOM keeps ONE #text node. */
TEST(layout_word_break_no_dom_growth) {
    DOMNode *root = parse_html("<p>one two three four five six seven</p>");
    /* Do NOT call split_text_nodes — layout handles word breaking now. */
    /* Count #text DOM nodes (should be 1). */
    int text_nodes = 0;
    DOMNode *stack[64]; int sp = 0; stack[sp++] = root;
    while (sp) {
        DOMNode *n = stack[--sp];
        if (n->name && strcmp(n->name, "#text") == 0) text_nodes++;
        for (int i = 0; i < n->children_count; i++) stack[sp++] = n->children[i];
    }
    CHECK_EQ_INT(1, text_nodes);   /* DOM not shredded */

    Layout *lay = layout_dom(root, NULL, 800);
    int word_boxes = 0;
    for (size_t i = 0; i < lay->count; i++)
        if (lay->boxes[i].node && lay->boxes[i].node->name &&
            strcmp(lay->boxes[i].node->name, "#text") == 0 &&
            lay->boxes[i].run_len > 0) word_boxes++;
    CHECK_EQ_INT(7, word_boxes);   /* 7 word boxes from 1 text node */
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

/* FEAT-011: <pre> preserves whitespace (no word breaking). */
TEST(layout_pre_preserves_text) {
    DOMNode *root = parse_html("<pre>code block here</pre>");
    Layout *lay = layout_dom(root, NULL, 800);
    /* the pre text should be a single whole-node box (run_len == 0) */
    int whole = 0, words = 0;
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->name && strcmp(b->node->name, "#text") == 0) {
            if (b->run_len == 0) whole++; else words++;
        }
    }
    CHECK(whole >= 1);
    CHECK_EQ_INT(0, words);
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

TEST(layout_null_dom_safe) {
    Layout *lay = layout_dom(NULL, NULL, 800);
    CHECK_NOT_NULL(lay);   /* empty layout, no crash */
    if (lay) CHECK_EQ_INT(0, (int)lay->count);
    free_layout(lay);
}

/* FIX-009: free_layout must NOT free the DOM; the DOM can be laid out many
   times and freed once. Run under ASan to catch UAF/double-free. */
TEST(layout_ownership_relayout_loop) {
    DOMNode *root = parse_html("<h1>Title</h1><p>Some text here</p>");
    split_text_nodes(root);
    for (int i = 0; i < 50; i++) {
        Layout *lay = layout_dom(root, NULL, 400 + i);
        CHECK_NOT_NULL(lay);
        free_layout(lay);          /* must not touch the DOM */
    }
    /* DOM is still valid after many layouts */
    CHECK_NOT_NULL(root->name);
    free_dom(root);                /* single free */
}

/* FEAT-006: margin/padding push subsequent content down. */
TEST(layout_box_model_spacing) {
    DOMNode *root = parse_html("<div><p>x</p></div>");
    DOMNode *div = dfs_tag(root, "div");
    CHECK_NOT_NULL(div);
    if (div) {
        ensure_computed_style(div);
        div->style->margin = strdup("40px");
        div->style->padding = strdup("20px");
    }
    split_text_nodes(root);
    Layout *lay = layout_dom(root, NULL, 800);
    /* find the div box and the inner text box */
    int div_y = -1, text_y = -1;
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->name && strcmp(b->node->name, "div") == 0) div_y = b->y;
        if (b->node && b->node->name && strcmp(b->node->name, "#text") == 0 && text_y < 0)
            text_y = b->y;
    }
    CHECK(div_y >= 40);              /* pushed down by margin-top */
    CHECK(text_y >= div_y + 20);     /* content offset by padding-top */
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

/* FEAT-002: text-align center/right shifts the line within the content box. */
TEST(layout_text_align_center) {
    DOMNode *root = parse_html("<p>hello world</p>");
    split_text_nodes(root);
    DOMNode *p = dfs_tag(root, "p");
    CHECK_NOT_NULL(p);
    if (p) {
        ensure_computed_style(p);
        p->style->text_align = strdup("center");
    }
    Layout *lay = layout_dom(root, NULL, 800);
    int first_x = -1;
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->name && strcmp(b->node->name, "#text") == 0) {
            first_x = b->x; break;
        }
    }
    CHECK(first_x > 30);   /* centered, not at left margin */
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}

TEST(layout_text_align_right) {
    DOMNode *root = parse_html("<p>hi there</p>");
    split_text_nodes(root);
    DOMNode *p = dfs_tag(root, "p");
    CHECK_NOT_NULL(p);
    if (p) { ensure_computed_style(p); p->style->text_align = strdup("right"); }
    Layout *lay = layout_dom(root, NULL, 800);
    int max_r = 0;
    for (size_t i = 0; i < lay->count; i++) {
        LayoutBox *b = &lay->boxes[i];
        if (b->node && b->node->name && strcmp(b->node->name, "#text") == 0) {
            int r = b->x + b->width; if (r > max_r) max_r = r;
        }
    }
    CHECK(max_r >= 760 && max_r <= 772);   /* flush to right margin (~770) */
    lay->dom = NULL;
    free_layout(lay);
    free_dom(root);
}
