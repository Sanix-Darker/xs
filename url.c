#include "url.h"
#include <string.h>
#include <stdio.h>

int url_is_absolute(const char* s) {
    return s && strstr(s, "://") != NULL;
}

void url_resolve(const char* base, const char* href, char* dst, size_t dst_sz) {
    if (!dst || dst_sz == 0) return;
    if (!href || !*href) { dst[0] = '\0'; return; }

    /* Absolute URL */
    if (strstr(href, "://")) {
        snprintf(dst, dst_sz, "%s", href);
        return;
    }

    if (!base || !*base) { snprintf(dst, dst_sz, "%s", href); return; }

    /* Root-relative: /path */
    if (href[0] == '/') {
        const char* p = strstr(base, "://");
        if (p) {
            p += 3;
            const char* slash = strchr(p, '/');
            if (slash) {
                int origin_len = (int)(slash - base);
                snprintf(dst, dst_sz, "%.*s%s", origin_len, base, href);
            } else {
                snprintf(dst, dst_sz, "%s%s", base, href);
            }
        } else if (strncmp(base, "file://", 7) == 0) {
            /* file://<root> + /abs -> treat path as filesystem-absolute */
            snprintf(dst, dst_sz, "file://%s", href);
        } else {
            snprintf(dst, dst_sz, "%s", href);
        }
        return;
    }

    /* Relative path: resolve against the base's directory. */
    const char* last_slash = strrchr(base, '/');
    const char* scheme_end = strstr(base, "://");
    if (scheme_end && last_slash && last_slash > scheme_end + 2) {
        int dir_len = (int)(last_slash - base) + 1;
        snprintf(dst, dst_sz, "%.*s%s", dir_len, base, href);
    } else if (strncmp(base, "file://", 7) == 0 && last_slash) {
        int dir_len = (int)(last_slash - base) + 1;
        snprintf(dst, dst_sz, "%.*s%s", dir_len, base, href);
    } else {
        snprintf(dst, dst_sz, "%s/%s", base, href);
    }
}
