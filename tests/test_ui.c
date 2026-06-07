#include "test_util.h"
#include "../ui.h"

TEST(ui_themes_exist) {
    CHECK(ui_theme_count() >= 2);
    CHECK_NOT_NULL(ui_theme(0));
    CHECK_NOT_NULL(ui_theme(1));
    /* wrap-around indexing */
    CHECK(ui_theme(ui_theme_count()) == ui_theme(0));
    CHECK(ui_theme(-1) == ui_theme(ui_theme_count() - 1));
}

TEST(ui_scrollbar_none_when_fits) {
    int ty, th;
    int need = ui_scrollbar_thumb(500, 700, 0, 40, 660, &ty, &th);
    CHECK_EQ_INT(0, need);   /* content shorter than viewport */
}

TEST(ui_scrollbar_at_top) {
    int ty, th;
    int need = ui_scrollbar_thumb(2000, 700, 0, 40, 660, &ty, &th);
    CHECK_EQ_INT(1, need);
    CHECK_EQ_INT(40, ty);    /* thumb at track top */
    CHECK(th > 0 && th < 660);
}

TEST(ui_scrollbar_at_bottom) {
    int ty, th;
    int content = 2000, view = 700, track_y = 40, track_h = 660;
    int max_scroll = content - view;     /* 1300 */
    int need = ui_scrollbar_thumb(content, view, -max_scroll, track_y, track_h, &ty, &th);
    CHECK_EQ_INT(1, need);
    /* thumb bottom should reach track bottom */
    CHECK_EQ_INT(track_y + track_h, ty + th);
}

TEST(ui_scrollbar_min_thumb) {
    int ty, th;
    /* huge content -> thumb clamped to a minimum height */
    ui_scrollbar_thumb(1000000, 700, 0, 40, 660, &ty, &th);
    CHECK(th >= 24);
}

TEST(ui_contrast_light_theme_readable) {
    /* Body text vs background should have decent contrast in light/kindle. */
    const Theme* t = ui_theme(0);
    double cr = ui_contrast_ratio(t->text, t->bg);
    CHECK(cr >= 4.5);   /* WCAG AA for normal text */
}

TEST(ui_contrast_dark_theme_readable) {
    /* find the dark theme by name */
    const Theme* dark = NULL;
    for (int i = 0; i < ui_theme_count(); i++) {
        const Theme* t = ui_theme(i);
        if (t->name[0] == 'd') { dark = t; break; }
    }
    CHECK_NOT_NULL(dark);
    if (dark) {
        double cr = ui_contrast_ratio(dark->text, dark->bg);
        CHECK(cr >= 4.5);
    }
}

TEST(ui_visited_set) {
    ui_visited_clear();
    CHECK_EQ_INT(0, ui_visited_contains("https://a.com"));
    ui_visited_add("https://a.com");
    CHECK_EQ_INT(1, ui_visited_contains("https://a.com"));
    CHECK_EQ_INT(0, ui_visited_contains("https://b.com"));
    /* adding twice is idempotent */
    ui_visited_add("https://a.com");
    CHECK_EQ_INT(1, ui_visited_contains("https://a.com"));
    ui_visited_clear();
    CHECK_EQ_INT(0, ui_visited_contains("https://a.com"));
}

TEST(ui_visited_bounded) {
    ui_visited_clear();
    char buf[32];
    for (int i = 0; i < 5000; i++) {     /* exceed VISITED_MAX (4096) */
        snprintf(buf, sizeof buf, "u%d", i);
        ui_visited_add(buf);
    }
    /* most recent should still be present; very old evicted */
    CHECK_EQ_INT(1, ui_visited_contains("u4999"));
    CHECK_EQ_INT(0, ui_visited_contains("u0"));
    ui_visited_clear();
}

TEST(ui_find_substring_ci) {
    CHECK_EQ_INT(1, ui_str_contains_ci("Hello World", "world"));
    CHECK_EQ_INT(1, ui_str_contains_ci("Hello World", "HELLO"));
    CHECK_EQ_INT(1, ui_str_contains_ci("abc", "abc"));
    CHECK_EQ_INT(0, ui_str_contains_ci("abc", "abcd"));
    CHECK_EQ_INT(0, ui_str_contains_ci("abc", ""));     /* empty needle */
    CHECK_EQ_INT(0, ui_str_contains_ci(NULL, "x"));
}

TEST(ui_find_scroll_center) {
    /* box at y=1000,h=20 in a 700 viewport, 3000 content. */
    int off = ui_scroll_to_center(1000, 20, 700, 3000);
    /* offset should be negative (scrolled down) and within range */
    CHECK(off <= 0);
    CHECK(off >= -(3000 - 700));
    /* box near top should not scroll past 0 */
    CHECK_EQ_INT(0, ui_scroll_to_center(0, 20, 700, 3000));
}
