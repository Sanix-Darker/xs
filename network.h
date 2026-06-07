#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>

void network_init(void);
void network_cleanup(void);

/* Rich fetch result (FIX-010 / FEAT-020). */
typedef struct {
    char*  body;            /* owned, NUL-terminated; NULL on failure */
    size_t length;
    long   status;          /* HTTP status (0 for file:// / N/A) */
    char   content_type[128];
    char   charset[32];
    char   final_url[2048]; /* effective URL after redirects */
    int    ok;              /* 1 if transfer succeeded and status < 400 */
} FetchResult;

/* Fetch a resource into `out`. Recognizes http(s):// via libcurl and
   file:// / bare local paths via the filesystem. Returns 1 on success
   (out->ok also set), 0 on hard failure. Caller must fetch_result_free(out). */
int  fetch_into(const char* url, FetchResult* out);
void fetch_result_free(FetchResult* out);

/* Legacy convenience: returns just the body (heap, caller frees), or NULL.
   Kept for callers that do not need status/content-type. */
char* fetch_url(const char* url);

/* Read a local file into a NUL-terminated heap buffer (size-capped). Returns
   NULL on failure. Exposed for testing and used by fetch for file://. */
char* fetch_file(const char* path);

/* Parse a Content-Type header value into type + charset (lowercased).
   Exposed for testing. type/charset buffers must be provided. */
void  parse_content_type(const char* header, char* type, size_t type_sz,
                         char* charset, size_t charset_sz);

#endif
