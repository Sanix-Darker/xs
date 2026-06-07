#include "test_util.h"
#include "../network.h"
#include <string.h>

TEST(net_content_type_basic) {
    char type[128], cs[32];
    parse_content_type("text/html; charset=utf-8", type, sizeof type, cs, sizeof cs);
    CHECK_STR_EQ("text/html", type);
    CHECK_STR_EQ("utf-8", cs);
}

TEST(net_content_type_latin1) {
    char type[128], cs[32];
    parse_content_type("text/html; charset=ISO-8859-1", type, sizeof type, cs, sizeof cs);
    CHECK_STR_EQ("text/html", type);
    CHECK_STR_EQ("iso-8859-1", cs);   /* lowercased */
}

TEST(net_content_type_no_charset) {
    char type[128], cs[32];
    parse_content_type("image/png", type, sizeof type, cs, sizeof cs);
    CHECK_STR_EQ("image/png", type);
    CHECK_STR_EQ("", cs);
}

TEST(net_content_type_quoted_charset) {
    char type[128], cs[32];
    parse_content_type("text/html; charset=\"UTF-8\"", type, sizeof type, cs, sizeof cs);
    CHECK_STR_EQ("text/html", type);
    CHECK_STR_EQ("utf-8", cs);
}

TEST(net_content_type_null) {
    char type[128] = "x", cs[32] = "y";
    parse_content_type(NULL, type, sizeof type, cs, sizeof cs);
    CHECK_STR_EQ("", type);
    CHECK_STR_EQ("", cs);
}

/* fetch_into on a local file sets a synthetic 200 + content-type. */
TEST(net_fetch_file_via_fetch_into) {
    /* Write a tiny temp file. */
    const char* path = "/tmp/xs_test_fetch.html";
    FILE* f = fopen(path, "wb");
    CHECK_NOT_NULL(f);
    if (f) { fputs("<p>hi</p>", f); fclose(f); }

    FetchResult fr;
    char url[256];
    snprintf(url, sizeof url, "file://%s", path);
    int ok = fetch_into(url, &fr);
    CHECK_EQ_INT(1, ok);
    if (ok) {
        CHECK_EQ_INT(200, (int)fr.status);
        CHECK_EQ_INT(1, fr.ok);
        CHECK_STR_EQ("text/html", fr.content_type);
        CHECK_NOT_NULL(fr.body);
        if (fr.body) CHECK_STR_EQ("<p>hi</p>", fr.body);
    }
    fetch_result_free(&fr);
    remove(path);
}

TEST(net_fetch_missing_file_fails) {
    FetchResult fr;
    int ok = fetch_into("file:///nonexistent/xs/zzz.html", &fr);
    CHECK_EQ_INT(0, ok);
    fetch_result_free(&fr);
}
