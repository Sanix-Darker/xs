/* Block-level tags (sorted, lowercase) — used by binary search in layout.c */
static const char *const block_tags[] = {
    "article", "aside", "blockquote", "dd", "details", "dialog",
    "div", "dl", "dt", "figcaption", "figure", "footer", "form",
    "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr",
    "li", "main", "nav", "ol", "p", "pre", "section", "summary",
    "table", "tbody", "td", "tfoot", "th", "thead", "tr", "ul"
};
#define BLOCK_TAGS_N  (sizeof block_tags / sizeof *block_tags)

/* Inline-level tags (sorted, lowercase) */
static const char *const inline_tags[] = {
    "#text", "a", "abbr", "b", "big", "br", "cite", "code",
    "em", "i", "img", "kbd", "label", "mark", "q", "s",
    "samp", "small", "span", "strong", "sub", "sup", "time", "u", "var"
};
#define INLINE_TAGS_N (sizeof inline_tags / sizeof *inline_tags)
