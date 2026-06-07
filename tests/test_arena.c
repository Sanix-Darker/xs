#include "test_util.h"
#include "../arena.h"
#include "../parser.h"
#include <string.h>
#include <stdint.h>

TEST(arena_basic_alloc) {
    Arena *a = arena_create(0);
    CHECK_NOT_NULL(a);
    char *p = arena_alloc(a, 10);
    CHECK_NOT_NULL(p);
    memset(p, 'x', 10);  /* must be writable */
    CHECK_EQ_INT(10, (int)arena_bytes(a));
    arena_destroy(a);
}

TEST(arena_alignment) {
    Arena *a = arena_create(0);
    /* Odd-sized allocations should still yield aligned pointers. */
    for (int i = 1; i <= 17; i++) {
        void *p = arena_alloc(a, (size_t)i);
        CHECK_NOT_NULL(p);
        uintptr_t addr = (uintptr_t)p;
        CHECK_EQ_INT(0, (int)(addr % sizeof(void *)));
    }
    arena_destroy(a);
}

TEST(arena_no_overlap) {
    Arena *a = arena_create(0);
    char *p1 = arena_alloc(a, 16);
    char *p2 = arena_alloc(a, 16);
    CHECK_NOT_NULL(p1);
    CHECK_NOT_NULL(p2);
    memset(p1, 1, 16);
    memset(p2, 2, 16);
    /* Distinct regions: writing p2 must not change p1's bytes. */
    CHECK_EQ_INT(1, p1[0]);
    CHECK_EQ_INT(2, p2[0]);
    CHECK(p2 >= p1 + 16 || p1 >= p2 + 16);
    arena_destroy(a);
}

TEST(arena_large_alloc_own_block) {
    Arena *a = arena_create(1024);  /* small default block */
    void *big = arena_alloc(a, 8192);  /* larger than default block */
    CHECK_NOT_NULL(big);
    memset(big, 7, 8192);
    arena_destroy(a);
}

TEST(arena_strdup_strndup) {
    Arena *a = arena_create(0);
    char *s = arena_strdup(a, "hello");
    CHECK_STR_EQ("hello", s);

    char *n = arena_strndup(a, "hello world", 5);
    CHECK_STR_EQ("hello", n);

    /* strndup stops early at an embedded NUL */
    char *e = arena_strndup(a, "ab\0cd", 5);
    CHECK_STR_EQ("ab", e);
    arena_destroy(a);
}

TEST(arena_calloc_zeroed) {
    Arena *a = arena_create(0);
    unsigned char *p = arena_calloc(a, 32, 1);
    CHECK_NOT_NULL(p);
    int allzero = 1;
    for (int i = 0; i < 32; i++) if (p[i] != 0) allzero = 0;
    CHECK(allzero);
    arena_destroy(a);
}

TEST(arena_many_allocs) {
    /* Stress: lots of small allocations across multiple blocks. */
    Arena *a = arena_create(256);
    for (int i = 0; i < 10000; i++) {
        int *p = arena_alloc(a, sizeof(int));
        CHECK_NOT_NULL(p);
        *p = i;
    }
    CHECK(arena_bytes(a) >= 10000 * sizeof(int));
    arena_destroy(a);
}

TEST(arena_destroy_null_safe) {
    arena_destroy(NULL);  /* must not crash */
    CHECK(1);
}

TEST(arena_realloc_grows_and_copies) {
    Arena *a = arena_create(0);
    int *p = arena_alloc(a, 4 * sizeof(int));
    for (int i = 0; i < 4; i++) p[i] = i + 1;
    int *q = arena_realloc(a, p, 4 * sizeof(int), 8 * sizeof(int));
    CHECK_NOT_NULL(q);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(i + 1, q[i]);  /* contents copied */
    arena_destroy(a);
}

/* MISS-007: parse a document into an arena, use it, free in one shot. Under
   ASan/LeakSan this proves arena-owned trees are not double-freed and free_dom
   is a correct no-op for them. */
TEST(arena_dom_parse_and_destroy) {
    Arena *a = arena_create(0);
    parser_set_arena(a);
    DOMNode *root = parse_html(
        "<html><body><div id=\"m\" class=\"a b\"><p>Hello <b>world</b></p>"
        "<a href=\"/x\">link</a></div></body></html>");
    parser_set_arena(NULL);   /* restore malloc for other tests */

    CHECK_NOT_NULL(root);
    /* find the div and verify attributes survived in the arena */
    DOMNode *div = NULL, *stack[128]; int sp = 0; stack[sp++] = root;
    while (sp) {
        DOMNode *n = stack[--sp];
        if (n->name && strcmp(n->name, "div") == 0) { div = n; break; }
        for (int i = 0; i < n->children_count; i++) stack[sp++] = n->children[i];
    }
    CHECK_NOT_NULL(div);
    if (div) {
        CHECK_STR_EQ("m", dom_id(div));
        CHECK(dom_has_class(div, "b"));
        CHECK(div->arena_owned);
    }

    free_dom(root);       /* must be a no-op for arena nodes (no double free) */
    arena_destroy(a);     /* frees the whole tree at once */
    CHECK(1);
}
