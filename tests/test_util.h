#ifndef XS_TEST_UTIL_H
#define XS_TEST_UTIL_H

/*
 * Tiny zero-dependency test harness (FIX-000).
 *
 * Usage:
 *   TEST(my_thing) {
 *       CHECK(1 + 1 == 2);
 *       CHECK_EQ_INT(2, 1 + 1);
 *       CHECK_STR_EQ("a", "a");
 *   }
 *
 * Each TEST() registers itself via a constructor. test_main.c provides main().
 * A test fails (but keeps running other tests) if any CHECK fails.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef struct XsTest {
    const char *name;
    void (*fn)(struct XsTest *self);
    int failures;
    struct XsTest *next;
} XsTest;

/* Registry (defined in test_main.c). */
void xs_test_register(XsTest *t);
extern int xs_test_total_failures;

#define TEST(NAME)                                                          \
    static void NAME##_fn(XsTest *self);                                    \
    static XsTest NAME##_test = { #NAME, NAME##_fn, 0, 0 };                 \
    __attribute__((constructor)) static void NAME##_register(void) {        \
        xs_test_register(&NAME##_test);                                     \
    }                                                                       \
    static void NAME##_fn(XsTest *self)

#define CHECK(COND)                                                         \
    do {                                                                    \
        if (!(COND)) {                                                      \
            self->failures++;                                               \
            xs_test_total_failures++;                                       \
            fprintf(stderr, "  FAIL %s:%d: CHECK(%s)\n",                    \
                    __FILE__, __LINE__, #COND);                            \
        }                                                                   \
    } while (0)

#define CHECK_EQ_INT(EXPECTED, ACTUAL)                                      \
    do {                                                                    \
        long long _e = (long long)(EXPECTED);                               \
        long long _a = (long long)(ACTUAL);                                 \
        if (_e != _a) {                                                     \
            self->failures++;                                               \
            xs_test_total_failures++;                                       \
            fprintf(stderr, "  FAIL %s:%d: expected %lld, got %lld (%s)\n", \
                    __FILE__, __LINE__, _e, _a, #ACTUAL);                  \
        }                                                                   \
    } while (0)

#define CHECK_STR_EQ(EXPECTED, ACTUAL)                                      \
    do {                                                                    \
        const char *_e = (EXPECTED);                                        \
        const char *_a = (ACTUAL);                                          \
        if (!_e || !_a || strcmp(_e, _a) != 0) {                            \
            self->failures++;                                               \
            xs_test_total_failures++;                                       \
            fprintf(stderr, "  FAIL %s:%d: expected \"%s\", got \"%s\"\n",  \
                    __FILE__, __LINE__, _e ? _e : "(null)",                \
                    _a ? _a : "(null)");                                   \
        }                                                                   \
    } while (0)

#define CHECK_STRN_EQ(EXPECTED, ACTUAL, N)                                  \
    do {                                                                    \
        const char *_e = (EXPECTED);                                        \
        const char *_a = (ACTUAL);                                          \
        if (!_e || !_a || strncmp(_e, _a, (N)) != 0) {                      \
            self->failures++;                                               \
            xs_test_total_failures++;                                       \
            fprintf(stderr, "  FAIL %s:%d: expected prefix \"%s\"\n",       \
                    __FILE__, __LINE__, _e ? _e : "(null)");               \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(EXPECTED, ACTUAL, TOL)                                   \
    do {                                                                    \
        double _e = (double)(EXPECTED);                                     \
        double _a = (double)(ACTUAL);                                       \
        if (fabs(_e - _a) > (double)(TOL)) {                                \
            self->failures++;                                               \
            xs_test_total_failures++;                                       \
            fprintf(stderr, "  FAIL %s:%d: expected ~%g, got %g\n",         \
                    __FILE__, __LINE__, _e, _a);                           \
        }                                                                   \
    } while (0)

#define CHECK_NOT_NULL(PTR)                                                 \
    do {                                                                    \
        if ((PTR) == NULL) {                                                \
            self->failures++;                                               \
            xs_test_total_failures++;                                       \
            fprintf(stderr, "  FAIL %s:%d: %s is NULL\n",                   \
                    __FILE__, __LINE__, #PTR);                             \
        }                                                                   \
    } while (0)

#endif /* XS_TEST_UTIL_H */
