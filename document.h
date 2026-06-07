#ifndef XS_DOCUMENT_H
#define XS_DOCUMENT_H

#include "parser.h"

/*
 * Document — the single, shared result of turning bytes into a ready-to-lay-out
 * tree (FEAT-001). Both the CLI (main.c) and the interactive renderer
 * (render.c) build pages through these functions so the pipeline can never
 * drift between the two paths (which is how FIX-002 originally crept in).
 *
 * NOTE: the DOM is still individually heap-allocated for now; document_free
 * frees it via free_dom. A later step routes allocation through an arena
 * (MISS-007) without changing this interface.
 */

typedef struct Document {
    DOMNode *root;       /* owned DOM tree */
    char    *title;      /* owned copy of <title> text, or NULL */
} Document;

/* Optional callback to fetch an external resource (e.g. <link> CSS). Given an
   absolute URL, returns a NUL-terminated heap buffer (caller frees) or NULL.
   When NULL, external stylesheets are skipped (FEAT-014). */
typedef char* (*ResourceFetcher)(const char* url);

/* Set the resource fetcher used for external stylesheets. Pass NULL to
   disable (the default). */
void document_set_fetcher(ResourceFetcher fetcher);

/* Build a Document from an HTML string (NUL-terminated). base_url is used for
   future relative-resolution needs and may be NULL. Returns NULL only on hard
   allocation failure (callers should fall back to an error document). */
Document *document_from_html(const char *html, const char *base_url);

/* Build a Document from raw bytes with a known/declared charset (MISS-006).
   The bytes are transcoded to UTF-8 first. charset may be NULL/empty (detected
   from a <meta charset> scan / UTF-8 validity). */
Document *document_from_bytes(const char *bytes, size_t len,
                              const char *base_url, const char *charset);

/* Free a Document and its DOM. Safe on NULL. */
void document_free(Document *doc);

/* Extract the concatenated text of the first <title> element (owned by caller,
   or NULL). Exposed for testing. */
char *document_extract_title(DOMNode *root);

#endif /* XS_DOCUMENT_H */
