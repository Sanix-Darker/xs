#ifndef XS_FORMS_H
#define XS_FORMS_H

#include "parser.h"
#include <stddef.h>

/*
 * Minimal forms support (MISS-004): GET submission of text inputs + buttons.
 * The pure pieces (URL-encoding, query-string building) are testable here.
 */

/* URL-encode `in` into `out` (percent-encoding, space->'+'). Returns the
   number of bytes written (excluding NUL), truncating to out_sz-1. */
size_t form_url_encode(const char* in, char* out, size_t out_sz);

/* Build a GET query string "name1=val1&name2=val2" from parallel arrays into
   `out`. Values are URL-encoded. Returns bytes written. */
size_t form_build_query(const char* const* names, const char* const* values,
                        int n, char* out, size_t out_sz);

/* Find the enclosing <form> of a node (walk parents), or NULL. */
DOMNode* form_owner(DOMNode* node);

/* Collect successful form controls (name+value) under `form` into the provided
   arrays (caller-owned storage). Returns the count (<= max). Currently gathers
   <input> elements with a name attribute (uses their `value` attribute). */
int form_collect_fields(DOMNode* form,
                        const char** names, const char** values, int max);

#endif /* XS_FORMS_H */
