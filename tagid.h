#ifndef XS_TAGID_H
#define XS_TAGID_H

/* Interned tag identifiers (FEAT-010). Mapping a tag name to a small enum once
   at parse time lets hot paths (layout classification) use integer compares
   instead of repeated strcmp. */

typedef enum {
    TAG_UNKNOWN = 0,
    TAG_TEXT,        /* #text */
    TAG_ROOT,
    TAG_A, TAG_B, TAG_BLOCKQUOTE, TAG_BODY, TAG_BR, TAG_CODE, TAG_DIV,
    TAG_EM, TAG_FORM, TAG_H1, TAG_H2, TAG_H3, TAG_H4, TAG_H5, TAG_H6,
    TAG_HEAD, TAG_HR, TAG_HTML, TAG_I, TAG_IMG, TAG_INPUT, TAG_LI, TAG_LINK,
    TAG_META, TAG_OL, TAG_P, TAG_PRE, TAG_SCRIPT, TAG_SMALL, TAG_SPAN,
    TAG_STRONG, TAG_STYLE, TAG_TABLE, TAG_TD, TAG_TH, TAG_TITLE, TAG_TR,
    TAG_UL, TAG_BUTTON, TAG_TEXTAREA,
    TAG__COUNT
} TagId;

/* Map a lowercased, NUL-terminated tag name to a TagId (TAG_UNKNOWN if none). */
TagId tagid_from_name(const char* name);

#endif /* XS_TAGID_H */
