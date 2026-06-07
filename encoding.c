#include "encoding.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Windows-1252 mapping for 0x80..0x9F to Unicode code points. The other bytes
   match Latin-1 (== code point). 0 entries are undefined; we map them to the
   replacement-ish code point 0xFFFD. */
static const unsigned cp1252_high[32] = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
};

Encoding encoding_from_label(const char* label) {
    if (!label || !*label) return ENC_UTF8;
    if (strcasecmp(label, "utf-8") == 0 || strcasecmp(label, "utf8") == 0)
        return ENC_UTF8;
    if (strcasecmp(label, "iso-8859-1") == 0 || strcasecmp(label, "latin1") == 0 ||
        strcasecmp(label, "iso8859-1") == 0 || strcasecmp(label, "l1") == 0)
        return ENC_LATIN1;
    if (strcasecmp(label, "windows-1252") == 0 || strcasecmp(label, "cp1252") == 0 ||
        strcasecmp(label, "win-1252") == 0)
        return ENC_CP1252;
    /* ascii is a subset of utf-8 */
    if (strcasecmp(label, "us-ascii") == 0 || strcasecmp(label, "ascii") == 0)
        return ENC_UTF8;
    return ENC_UTF8;
}

int utf8_validate(const char* s, size_t len) {
    const unsigned char* p = (const unsigned char*)s;
    size_t i = 0;
    while (i < len) {
        unsigned char c = p[i];
        if (c < 0x80) { i++; continue; }
        int extra;
        unsigned cp;
        if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
        else return 0;
        if (i + (size_t)extra >= len) return 0;
        for (int k = 1; k <= extra; k++) {
            if ((p[i + k] & 0xC0) != 0x80) return 0;
            cp = (cp << 6) | (p[i + k] & 0x3F);
        }
        /* reject overlong / out of range */
        if (cp > 0x10FFFF) return 0;
        if (extra == 1 && cp < 0x80) return 0;
        if (extra == 2 && cp < 0x800) return 0;
        if (extra == 3 && cp < 0x10000) return 0;
        i += extra + 1;
    }
    return 1;
}

/* Scan first bytes for <meta charset=...> or http-equiv content charset. */
static int meta_charset(const char* body, size_t len, char* out, size_t out_sz) {
    size_t scan = len < 2048 ? len : 2048;
    for (size_t i = 0; i + 6 < scan; i++) {
        if (strncasecmp(body + i, "charset", 7) == 0) {
            const char* p = body + i + 7;
            const char* end = body + scan;
            while (p < end && (*p == ' ' || *p == '=' || *p == '"' || *p == '\'')) p++;
            size_t j = 0;
            while (p < end && *p != '"' && *p != '\'' && *p != '>' &&
                   *p != ';' && *p != ' ' && j + 1 < out_sz) {
                out[j++] = *p++;
            }
            out[j] = '\0';
            if (j > 0) return 1;
        }
    }
    return 0;
}

Encoding encoding_detect(const char* header_charset, const char* body, size_t len) {
    /* 1. BOM sniff */
    if (len >= 3 && (unsigned char)body[0] == 0xEF &&
        (unsigned char)body[1] == 0xBB && (unsigned char)body[2] == 0xBF)
        return ENC_UTF8;

    /* 2. HTTP header charset */
    if (header_charset && *header_charset)
        return encoding_from_label(header_charset);

    /* 3. <meta charset> */
    char label[64];
    if (body && meta_charset(body, len, label, sizeof label))
        return encoding_from_label(label);

    /* 4. If not valid UTF-8, fall back to Windows-1252 (superset of Latin-1
          for the common punctuation range). */
    if (body && !utf8_validate(body, len))
        return ENC_CP1252;

    return ENC_UTF8;
}

static char* append_cp(char* out, size_t* n, unsigned cp) {
    /* caller guarantees space for up to 4 bytes */
    if (cp < 0x80) {
        out[(*n)++] = (char)cp;
    } else if (cp < 0x800) {
        out[(*n)++] = (char)(0xC0 | (cp >> 6));
        out[(*n)++] = (char)(0x80 | (cp & 0x3F));
    } else {
        out[(*n)++] = (char)(0xE0 | (cp >> 12));
        out[(*n)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[(*n)++] = (char)(0x80 | (cp & 0x3F));
    }
    return out;
}

char* encoding_to_utf8(Encoding enc, const char* in, size_t len, size_t* out_len) {
    if (!in) return NULL;

    if (enc == ENC_UTF8) {
        char* out = xs_malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, in, len);
        out[len] = '\0';
        if (out_len) *out_len = len;
        return out;
    }

    /* Latin-1 / CP1252: each input byte -> up to 3 UTF-8 bytes. */
    char* out = xs_malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t n = 0;
    const unsigned char* p = (const unsigned char*)in;
    for (size_t i = 0; i < len; i++) {
        unsigned c = p[i];
        unsigned cp;
        if (enc == ENC_CP1252 && c >= 0x80 && c <= 0x9F)
            cp = cp1252_high[c - 0x80];
        else
            cp = c;   /* Latin-1: byte == code point */
        append_cp(out, &n, cp);
    }
    out[n] = '\0';
    if (out_len) *out_len = n;
    return out;
}
