#include "test_util.h"
#include "../encoding.h"
#include <string.h>
#include <stdlib.h>

TEST(enc_label_map) {
    CHECK_EQ_INT(ENC_UTF8,   encoding_from_label("utf-8"));
    CHECK_EQ_INT(ENC_UTF8,   encoding_from_label("UTF8"));
    CHECK_EQ_INT(ENC_LATIN1, encoding_from_label("ISO-8859-1"));
    CHECK_EQ_INT(ENC_CP1252, encoding_from_label("windows-1252"));
    CHECK_EQ_INT(ENC_UTF8,   encoding_from_label("unknown-thing"));
    CHECK_EQ_INT(ENC_UTF8,   encoding_from_label(NULL));
}

TEST(enc_latin1_to_utf8) {
    /* 0xE9 == 'é' in Latin-1 -> UTF-8 0xC3 0xA9 */
    const char in[] = { (char)0xE9, 0 };
    size_t out_len = 0;
    char *out = encoding_to_utf8(ENC_LATIN1, in, 1, &out_len);
    CHECK_NOT_NULL(out);
    if (out) {
        CHECK_EQ_INT(2, (int)out_len);
        CHECK_EQ_INT(0xC3, (unsigned char)out[0]);
        CHECK_EQ_INT(0xA9, (unsigned char)out[1]);
        free(out);
    }
}

TEST(enc_cp1252_smartquotes) {
    /* 0x93/0x94 are curly double quotes in CP1252 (U+201C / U+201D). */
    const char in[] = { (char)0x93, (char)0x94, 0 };
    size_t out_len = 0;
    char *out = encoding_to_utf8(ENC_CP1252, in, 2, &out_len);
    CHECK_NOT_NULL(out);
    if (out) {
        /* U+201C = E2 80 9C, U+201D = E2 80 9D */
        CHECK_EQ_INT(6, (int)out_len);
        CHECK_EQ_INT(0xE2, (unsigned char)out[0]);
        CHECK_EQ_INT(0x80, (unsigned char)out[1]);
        CHECK_EQ_INT(0x9C, (unsigned char)out[2]);
        CHECK_EQ_INT(0x9D, (unsigned char)out[5]);
        free(out);
    }
}

TEST(enc_utf8_passthrough) {
    const char *in = "café";   /* already UTF-8 */
    size_t out_len = 0;
    char *out = encoding_to_utf8(ENC_UTF8, in, strlen(in), &out_len);
    CHECK_NOT_NULL(out);
    if (out) { CHECK_STR_EQ(in, out); free(out); }
}

TEST(enc_utf8_validate) {
    CHECK(utf8_validate("hello", 5));
    CHECK(utf8_validate("caf\xC3\xA9", 5));      /* café */
    CHECK_EQ_INT(0, utf8_validate("\xC3", 1));    /* truncated sequence */
    CHECK_EQ_INT(0, utf8_validate("\xFF", 1));    /* invalid lead byte */
}

TEST(enc_detect_header_wins) {
    const char *body = "<html></html>";
    CHECK_EQ_INT(ENC_LATIN1, encoding_detect("iso-8859-1", body, strlen(body)));
    CHECK_EQ_INT(ENC_CP1252, encoding_detect("windows-1252", body, strlen(body)));
}

TEST(enc_detect_meta) {
    const char *body = "<head><meta charset=\"ISO-8859-1\"></head>";
    CHECK_EQ_INT(ENC_LATIN1, encoding_detect(NULL, body, strlen(body)));
}

TEST(enc_detect_bom) {
    const char body[] = { (char)0xEF, (char)0xBB, (char)0xBF, 'h', 'i', 0 };
    CHECK_EQ_INT(ENC_UTF8, encoding_detect(NULL, body, 5));
}

TEST(enc_detect_invalid_utf8_fallback) {
    /* Body with a lone 0xE9 (not valid UTF-8), no header/meta -> CP1252. */
    const char body[] = { '<', 'p', '>', (char)0xE9, '<', '/', 'p', '>', 0 };
    CHECK_EQ_INT(ENC_CP1252, encoding_detect(NULL, body, 8));
}
