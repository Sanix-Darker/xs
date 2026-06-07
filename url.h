#ifndef XS_URL_H
#define XS_URL_H

#include <stddef.h>

/* Resolve `href` against `base` into `dst` (absolute URL).
   Handles absolute (scheme://), root-relative (/path), and relative paths,
   plus file:// bases. dst is always NUL-terminated. */
void url_resolve(const char* base, const char* href, char* dst, size_t dst_sz);

/* 1 if the string looks like an absolute URL (contains "://"). */
int  url_is_absolute(const char* s);

#endif /* XS_URL_H */
