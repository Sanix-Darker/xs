#include "ui.h"
#include <math.h>
#include <stdlib.h>

static const Theme g_themes[] = {
    { "kindle",
      {250,248,245}, {30,30,30}, {15,15,15}, {20,70,180}, {240,240,240}, {180,180,180} },
    { "light",
      {255,255,255}, {20,20,20}, {0,0,0}, {0,0,238}, {245,245,245}, {170,170,170} },
    { "dark",
      {24,24,28}, {220,220,220}, {245,245,245}, {120,170,255}, {40,40,46}, {90,90,100} },
    { "sepia",
      {244,236,216}, {60,50,38}, {40,32,24}, {120,70,30}, {236,226,200}, {180,165,130} },
};

int ui_theme_count(void) {
    return (int)(sizeof(g_themes) / sizeof(g_themes[0]));
}

const Theme* ui_theme(int index) {
    int n = ui_theme_count();
    int i = ((index % n) + n) % n;
    return &g_themes[i];
}

int ui_scrollbar_thumb(int content_h, int view_h, int scroll_offset,
                       int track_y, int track_h, int* thumb_y, int* thumb_h) {
    if (content_h <= view_h || view_h <= 0 || track_h <= 0) {
        if (thumb_y) *thumb_y = track_y;
        if (thumb_h) *thumb_h = track_h;
        return 0;   /* no scrollbar needed */
    }

    /* thumb height proportional to visible fraction, min 24px */
    long th = (long)track_h * view_h / content_h;
    if (th < 24) th = 24;
    if (th > track_h) th = track_h;

    /* scroll_offset ranges [-(content_h - view_h), 0]; map to [0, track_h-th] */
    int max_scroll = content_h - view_h;       /* > 0 here */
    int pos = -scroll_offset;                   /* 0..max_scroll */
    if (pos < 0) pos = 0;
    if (pos > max_scroll) pos = max_scroll;
    long ty = (long)(track_h - th) * pos / max_scroll;

    if (thumb_y) *thumb_y = track_y + (int)ty;
    if (thumb_h) *thumb_h = (int)th;
    return 1;
}

static double chan(double c) {
    c /= 255.0;
    return (c <= 0.03928) ? (c / 12.92) : pow((c + 0.055) / 1.055, 2.4);
}

double ui_luminance(RGB c) {
    return 0.2126 * chan(c.r) + 0.7152 * chan(c.g) + 0.0722 * chan(c.b);
}

double ui_contrast_ratio(RGB a, RGB b) {
    double la = ui_luminance(a), lb = ui_luminance(b);
    double hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

/* --- Visited-URL set (FEAT-012): bounded ring of hashes + strings --- */
#define VISITED_MAX 4096

static char* g_visited[VISITED_MAX];
static int   g_visited_head = 0;   /* next slot to overwrite (ring) */
static int   g_visited_count = 0;

static char* dup_cstr(const char* s) {
    size_t n = 0; while (s[n]) n++;
    char* o = (char*)malloc(n + 1);
    if (o) { for (size_t i = 0; i <= n; i++) o[i] = s[i]; }
    return o;
}

int ui_visited_contains(const char* url) {
    if (!url) return 0;
    for (int i = 0; i < g_visited_count; i++) {
        if (g_visited[i]) {
            const char* a = g_visited[i]; const char* b = url;
            while (*a && *b && *a == *b) { a++; b++; }
            if (*a == '\0' && *b == '\0') return 1;
        }
    }
    return 0;
}

void ui_visited_add(const char* url) {
    if (!url || !*url) return;
    if (ui_visited_contains(url)) return;
    /* free the slot we are about to overwrite (ring eviction) */
    if (g_visited[g_visited_head]) free(g_visited[g_visited_head]);
    g_visited[g_visited_head] = dup_cstr(url);
    g_visited_head = (g_visited_head + 1) % VISITED_MAX;
    if (g_visited_count < VISITED_MAX) g_visited_count++;
}

void ui_visited_clear(void) {
    for (int i = 0; i < VISITED_MAX; i++) {
        if (g_visited[i]) { free(g_visited[i]); g_visited[i] = NULL; }
    }
    g_visited_head = 0;
    g_visited_count = 0;
}

/* --- Find-in-page helpers (FEAT-008) --- */

static int lc(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int ui_str_contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle || !*needle) return 0;
    for (const char* h = haystack; *h; h++) {
        const char* a = h;
        const char* b = needle;
        while (*a && *b && lc((unsigned char)*a) == lc((unsigned char)*b)) { a++; b++; }
        if (*b == '\0') return 1;
    }
    return 0;
}

int ui_scroll_to_center(int box_y, int box_h, int view_h, int content_h) {
    int target = box_y - (view_h - box_h) / 2;   /* content-space top of viewport */
    if (target < 0) target = 0;
    int max_scroll = content_h - view_h;
    if (max_scroll < 0) max_scroll = 0;
    if (target > max_scroll) target = max_scroll;
    return -target;   /* renderer uses negative offset to scroll down */
}
