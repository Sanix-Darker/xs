#include "test_util.h"
#include "../url.h"
#include <string.h>

TEST(url_absolute) {
    char dst[256];
    url_resolve("https://a.com/x/", "https://b.com/y", dst, sizeof dst);
    CHECK_STR_EQ("https://b.com/y", dst);
}

TEST(url_root_relative) {
    char dst[256];
    url_resolve("https://a.com/x/y/z.html", "/abs/path", dst, sizeof dst);
    CHECK_STR_EQ("https://a.com/abs/path", dst);
}

TEST(url_relative) {
    char dst[256];
    url_resolve("https://a.com/x/y/z.html", "w.css", dst, sizeof dst);
    CHECK_STR_EQ("https://a.com/x/y/w.css", dst);
}

TEST(url_relative_root_page) {
    char dst[256];
    url_resolve("https://a.com/page.html", "style.css", dst, sizeof dst);
    CHECK_STR_EQ("https://a.com/style.css", dst);
}

TEST(url_file_relative) {
    char dst[256];
    url_resolve("file:///home/u/site/index.html", "site.css", dst, sizeof dst);
    CHECK_STR_EQ("file:///home/u/site/site.css", dst);
}

TEST(url_is_absolute) {
    CHECK(url_is_absolute("https://x"));
    CHECK_EQ_INT(0, url_is_absolute("/path"));
    CHECK_EQ_INT(0, url_is_absolute("rel.css"));
}

TEST(url_empty_href) {
    char dst[256];
    url_resolve("https://a.com/", "", dst, sizeof dst);
    CHECK_STR_EQ("", dst);
}
