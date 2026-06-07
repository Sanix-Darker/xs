#include "tagid.h"
#include <string.h>

/* Sorted (by name) table for binary search. Names are lowercase. */
struct TagEntry { const char* name; TagId id; };
static const struct TagEntry g_tags[] = {
    { "#text", TAG_TEXT },
    { "a", TAG_A },
    { "b", TAG_B },
    { "blockquote", TAG_BLOCKQUOTE },
    { "body", TAG_BODY },
    { "br", TAG_BR },
    { "button", TAG_BUTTON },
    { "code", TAG_CODE },
    { "div", TAG_DIV },
    { "em", TAG_EM },
    { "form", TAG_FORM },
    { "h1", TAG_H1 },
    { "h2", TAG_H2 },
    { "h3", TAG_H3 },
    { "h4", TAG_H4 },
    { "h5", TAG_H5 },
    { "h6", TAG_H6 },
    { "head", TAG_HEAD },
    { "hr", TAG_HR },
    { "html", TAG_HTML },
    { "i", TAG_I },
    { "img", TAG_IMG },
    { "input", TAG_INPUT },
    { "li", TAG_LI },
    { "link", TAG_LINK },
    { "meta", TAG_META },
    { "ol", TAG_OL },
    { "p", TAG_P },
    { "pre", TAG_PRE },
    { "root", TAG_ROOT },
    { "script", TAG_SCRIPT },
    { "small", TAG_SMALL },
    { "span", TAG_SPAN },
    { "strong", TAG_STRONG },
    { "style", TAG_STYLE },
    { "table", TAG_TABLE },
    { "td", TAG_TD },
    { "textarea", TAG_TEXTAREA },
    { "th", TAG_TH },
    { "title", TAG_TITLE },
    { "tr", TAG_TR },
    { "ul", TAG_UL },
};
#define N_TAGS (sizeof(g_tags) / sizeof(g_tags[0]))

TagId tagid_from_name(const char* name) {
    if (!name) return TAG_UNKNOWN;
    size_t lo = 0, hi = N_TAGS;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        int c = strcmp(name, g_tags[mid].name);
        if (c == 0) return g_tags[mid].id;
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return TAG_UNKNOWN;
}
