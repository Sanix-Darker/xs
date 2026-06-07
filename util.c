#include "util.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Optional test hook: force allocation failures deterministically.    */
#ifdef XS_TESTING
static long g_alloc_count = 0;
static long g_alloc_fail_at = 0;  /* 0 = never fail */

void xs_test_set_alloc_fail(long nth_alloc_fails) { g_alloc_fail_at = nth_alloc_fails; }
long xs_test_alloc_count(void) { return g_alloc_count; }
void xs_test_reset_alloc_count(void) { g_alloc_count = 0; g_alloc_fail_at = 0; }

static int alloc_should_fail(void) {
    long n = ++g_alloc_count;
    return (g_alloc_fail_at > 0 && n == g_alloc_fail_at);
}
#else
static inline int alloc_should_fail(void) { return 0; }
#endif

/* ------------------------------------------------------------------ */

void *xs_malloc(size_t n) {
    if (alloc_should_fail()) return NULL;
    return malloc(n);
}

void *xs_calloc(size_t count, size_t size) {
    if (alloc_should_fail()) return NULL;
    return calloc(count, size);
}

void *xs_realloc(void *p, size_t n) {
    if (alloc_should_fail()) return NULL;
    return realloc(p, n);
}

char *xs_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = xs_malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

char *xs_strndup(const char *s, size_t n) {
    if (!s) return NULL;
    /* copy at most n bytes, stopping at a NUL if one occurs earlier */
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;
    char *out = xs_malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

int xs_grow(void **ptr, size_t *cap, size_t need, size_t elem_size) {
    if (need <= *cap) return 1;
    size_t newcap = *cap ? *cap : 8;
    while (newcap < need) {
        /* geometric growth with overflow guard */
        if (newcap > (size_t)-1 / 2) { newcap = need; break; }
        newcap <<= 1;
    }
    if (elem_size != 0 && newcap > (size_t)-1 / elem_size) return 0; /* overflow */
    void *tmp = xs_realloc(*ptr, newcap * elem_size);
    if (!tmp) return 0;       /* leave ptr and cap unchanged on failure */
    *ptr = tmp;
    *cap = newcap;
    return 1;
}
