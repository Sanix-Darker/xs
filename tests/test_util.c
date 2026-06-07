#include "test_util.h"
#include "../util.h"
#include <string.h>

TEST(util_isspace_unsigned) {
    /* High-bit byte (e.g. UTF-8 lead) must not be treated as space and must
       not invoke UB (FIX-004). */
    CHECK_EQ_INT(0, xs_isspace((char)0xC3));   /* 'Ã' lead byte */
    CHECK(xs_isspace(' '));
    CHECK(xs_isspace('\t'));
    CHECK(xs_isspace('\n'));
    CHECK_EQ_INT(0, xs_isspace('a'));
}

TEST(util_html_space) {
    CHECK(xs_is_html_space(' '));
    CHECK(xs_is_html_space('\t'));
    CHECK(xs_is_html_space('\n'));
    CHECK(xs_is_html_space('\r'));
    CHECK(xs_is_html_space('\f'));
    CHECK_EQ_INT(0, xs_is_html_space('x'));
    CHECK_EQ_INT(0, xs_is_html_space((char)0xA0)); /* nbsp is NOT html space */
}

TEST(util_strndup) {
    char *s = xs_strndup("hello world", 5);
    CHECK_STR_EQ("hello", s);
    free(s);

    char *e = xs_strndup("ab\0cd", 5);
    CHECK_STR_EQ("ab", e);
    free(e);
}

TEST(util_grow) {
    int *arr = NULL;
    size_t cap = 0;
    CHECK(xs_grow((void **)&arr, &cap, 1, sizeof(int)));
    CHECK(cap >= 1);
    arr[0] = 42;
    size_t old = cap;
    CHECK(xs_grow((void **)&arr, &cap, old + 1, sizeof(int)));
    CHECK(cap > old);
    CHECK_EQ_INT(42, arr[0]);  /* contents preserved across grow */
    free(arr);
}

#ifdef XS_TESTING
TEST(util_alloc_fail_hook) {
    xs_test_reset_alloc_count();
    xs_test_set_alloc_fail(1);     /* first alloc fails */
    void *p = xs_malloc(8);
    CHECK(p == NULL);
    /* subsequent allocs succeed */
    void *q = xs_malloc(8);
    CHECK_NOT_NULL(q);
    free(q);
    xs_test_reset_alloc_count();
}

TEST(util_grow_fail_preserves) {
    xs_test_reset_alloc_count();
    int *arr = NULL;
    size_t cap = 0;
    CHECK(xs_grow((void **)&arr, &cap, 4, sizeof(int)));
    arr[0] = 99;
    size_t saved_cap = cap;
    /* Reset the counter, then force the very next allocation to fail; arr/cap
       must be left intact. */
    xs_test_reset_alloc_count();
    xs_test_set_alloc_fail(1);
    int ok = xs_grow((void **)&arr, &cap, saved_cap + 100, sizeof(int));
    CHECK_EQ_INT(0, ok);
    CHECK_EQ_INT((int)saved_cap, (int)cap);
    CHECK_EQ_INT(99, arr[0]);
    xs_test_reset_alloc_count();
    free(arr);
}
#endif
