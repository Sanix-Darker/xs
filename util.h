#ifndef XS_UTIL_H
#define XS_UTIL_H

#include <stddef.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Resource caps (FIX-015). Override at compile time with -DXS_MAX_*   */
#ifndef XS_MAX_DOCUMENT_BYTES
#define XS_MAX_DOCUMENT_BYTES (32u * 1024u * 1024u)   /* 32 MB */
#endif
#ifndef XS_MAX_NODES
#define XS_MAX_NODES (2000000u)
#endif
#ifndef XS_MAX_DEPTH
#define XS_MAX_DEPTH (512)
#endif

/* ------------------------------------------------------------------ */
/* Safe ctype wrappers (FIX-004): always pass through unsigned char.   */
static inline int xs_isspace(int c) { return isspace((unsigned char)c); }
static inline int xs_isdigit(int c) { return isdigit((unsigned char)c); }
static inline int xs_isalpha(int c) { return isalpha((unsigned char)c); }
static inline int xs_tolower(int c) { return tolower((unsigned char)c); }

/* HTML whitespace per spec subset: space, tab, LF, CR, FF. */
static inline int xs_is_html_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/* ------------------------------------------------------------------ */
/* Safe allocation wrappers (FIX-005).                                 */
/* These never abort; on failure they return NULL and callers must     */
/* handle it by failing the operation gracefully.                      */
/*                                                                     */
/* A test hook (XS_TEST_ALLOC_FAIL) lets unit tests force failures.    */
void *xs_malloc(size_t n);
void *xs_calloc(size_t count, size_t size);
void *xs_realloc(void *p, size_t n);
char *xs_strdup(const char *s);
char *xs_strndup(const char *s, size_t n);

/* Grow *ptr (array of `elem_size` items) to hold at least `need`      */
/* items, updating *cap. Returns 1 on success, 0 on failure (in which  */
/* case *ptr and *cap are left unchanged so the caller can recover).   */
int xs_grow(void **ptr, size_t *cap, size_t need, size_t elem_size);

/* Test-only: when > 0, the Nth allocation (1-based) fails. 0 = off.   */
#ifdef XS_TESTING
void xs_test_set_alloc_fail(long nth_alloc_fails);
long xs_test_alloc_count(void);
void xs_test_reset_alloc_count(void);
#endif

#endif /* XS_UTIL_H */
