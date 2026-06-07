#include "test_util.h"
#include "../style.h"

TEST(style_length_px) {
    Length l = style_parse_length("12px");
    CHECK_EQ_INT(LEN_PX, l.unit);
    CHECK_NEAR(12.0, l.value, 0.01);
}

TEST(style_length_percent) {
    Length l = style_parse_length("50%");
    CHECK_EQ_INT(LEN_PERCENT, l.unit);
    CHECK_NEAR(50.0, l.value, 0.01);
    CHECK_EQ_INT(300, style_resolve_length(l, 600, 16, 0));
}

TEST(style_length_em) {
    Length l = style_parse_length("1.5em");
    CHECK_EQ_INT(LEN_EM, l.unit);
    CHECK_EQ_INT(24, style_resolve_length(l, 600, 16, 0));
}

TEST(style_length_auto_and_unitless) {
    CHECK_EQ_INT(LEN_AUTO, style_parse_length("auto").unit);
    CHECK_EQ_INT(LEN_AUTO, style_parse_length("").unit);
    Length u = style_parse_length("20");
    CHECK_EQ_INT(LEN_PX, u.unit);   /* unitless treated as px */
    CHECK_EQ_INT(20, style_resolve_length(u, 600, 16, 0));
}

TEST(style_length_pt) {
    Length l = style_parse_length("72pt");   /* 72pt == 96px */
    CHECK_EQ_INT(LEN_PX, l.unit);
    CHECK_EQ_INT(96, style_resolve_length(l, 0, 16, 0));
}

TEST(style_color_hex3_eq_hex6) {
    Color a, b;
    CHECK(style_parse_color("#fff", &a));
    CHECK(style_parse_color("#ffffff", &b));
    CHECK_EQ_INT(255, a.r); CHECK_EQ_INT(255, a.g); CHECK_EQ_INT(255, a.b);
    CHECK_EQ_INT(b.r, a.r); CHECK_EQ_INT(b.g, a.g); CHECK_EQ_INT(b.b, a.b);
}

TEST(style_color_rgb) {
    Color c;
    CHECK(style_parse_color("rgb(255, 0, 0)", &c));
    CHECK_EQ_INT(255, c.r); CHECK_EQ_INT(0, c.g); CHECK_EQ_INT(0, c.b);
    CHECK_EQ_INT(255, c.a);
}

TEST(style_color_rgba) {
    Color c;
    CHECK(style_parse_color("rgba(0, 128, 0, 0.5)", &c));
    CHECK_EQ_INT(0, c.r); CHECK_EQ_INT(128, c.g); CHECK_EQ_INT(0, c.b);
    CHECK(c.a >= 120 && c.a <= 135);   /* ~127 */
}

TEST(style_color_named) {
    Color c;
    CHECK(style_parse_color("red", &c));
    CHECK_EQ_INT(255, c.r); CHECK_EQ_INT(0, c.g); CHECK_EQ_INT(0, c.b);
    CHECK(style_parse_color("navy", &c));
    CHECK_EQ_INT(0, c.r); CHECK_EQ_INT(0, c.g); CHECK_EQ_INT(128, c.b);
}

TEST(style_color_bad) {
    Color c = { 1, 2, 3, 4 };
    CHECK_EQ_INT(0, style_parse_color("notacolor", &c));
    CHECK_EQ_INT(1, c.r);  /* unchanged on failure */
}

TEST(style_align) {
    CHECK_EQ_INT(ALIGN_CENTER, style_parse_align("center"));
    CHECK_EQ_INT(ALIGN_RIGHT,  style_parse_align("right"));
    CHECK_EQ_INT(ALIGN_LEFT,   style_parse_align("left"));
    CHECK_EQ_INT(ALIGN_LEFT,   style_parse_align("bogus"));
}
