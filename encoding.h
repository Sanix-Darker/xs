#ifndef XS_ENCODING_H
#define XS_ENCODING_H

#include <stddef.h>

/*
 * Character encoding detection + transcoding to UTF-8 (MISS-006).
 * Supported: UTF-8 (passthrough), ISO-8859-1 (Latin-1), Windows-1252.
 */

typedef enum {
    ENC_UTF8 = 0,
    ENC_LATIN1,
    ENC_CP1252,
} Encoding;

/* Map a charset label (e.g. "utf-8", "iso-8859-1", "windows-1252") to an
   Encoding. Unknown -> ENC_UTF8. */
Encoding encoding_from_label(const char* label);

/* Detect encoding: prefers the HTTP charset, then a <meta charset> scan of the
   first bytes, then BOM, else UTF-8. header_charset may be NULL/empty. */
Encoding encoding_detect(const char* header_charset, const char* body, size_t len);

/* Transcode `in` (len bytes) from `enc` to a freshly allocated NUL-terminated
   UTF-8 string. For ENC_UTF8 this validates/copies. Returns NULL on alloc
   failure. *out_len (optional) receives the output byte length. */
char* encoding_to_utf8(Encoding enc, const char* in, size_t len, size_t* out_len);

/* Returns 1 if the buffer is valid UTF-8. */
int   utf8_validate(const char* s, size_t len);

#endif /* XS_ENCODING_H */
