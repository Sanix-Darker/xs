static const char *const block_tags[] = {
    "div", "h1", "h2", "h3", "h4", "h5", "nav", "p", "summary"
};
#define BLOCK_TAGS_N  (sizeof block_tags / sizeof *block_tags)

static const char *const inline_tags[] = {
    "#text", "a", "b", "code", "i", "small", "span", "strong", "u", "ul"
};
#define INLINE_TAGS_N (sizeof inline_tags / sizeof *inline_tags)
