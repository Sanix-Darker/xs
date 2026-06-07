// Build: gcc -O2 -Wall -Wextra -pedantic xs_browser_optimized.c -lSDL2 -lSDL2_ttf -o xs
//-----------------------------------------------------------------------------
#include "render.h"
#include "layout.h"
#include "parser.h"
#include "css.h"
#include "style.h"
#include "network.h"
#include "javascript.h"
#include "document.h"
#include "url.h"
#include "ui.h"
#include "forms.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// ---------------------------------------------------------------------------
//     CONFIG & GLOBALS
// ---------------------------------------------------------------------------
#define SEARCH_BAR_HEIGHT     40
#define SEARCH_BUFFER_SIZE    1024
#define SCROLL_STEP           20
#define HISTORY_MAX           64

/* Kindle-like colors */
#define BG_R 250
#define BG_G 248
#define BG_B 245

static int  window_w              = 950;
static int  window_h              = 700;
static int  scroll_offset         = 0;
static char search_query[SEARCH_BUFFER_SIZE] = "";
static char current_url[2048]     = "";
static Layout *currentLayout      = NULL;
static DOMNode *currentDom        = NULL;   /* FIX-009: render owns the DOM */
static char current_title[512]    = "";
static int  g_theme_index         = 0;
static SDL_Window *g_window        = NULL;
static int  content_height        = 0;
static bool needs_redraw          = true;
static bool search_focused        = true;

/* Find-in-page state (FEAT-008). */
static bool find_active           = false;
static char find_query[256]       = "";
static int  find_current          = 0;   /* index into match list */

/* Back/forward history */
static char *history_urls[HISTORY_MAX];
static int   history_count = 0;
static int   history_pos   = -1;

// ---------------------------------------------------------------------------
//     FONT CACHE  (size + bold + italic -> TTF_Font*)
// ---------------------------------------------------------------------------
#define FONT_CACHE_MAX 24

typedef struct {
    int        size;
    int        bold;
    int        italic;
    TTF_Font  *font;
} FontCacheEntry;

static FontCacheEntry font_cache[FONT_CACHE_MAX];
static int font_cache_count = 0;

static TTF_Font *load_font_path(const char *filename, int size)
{
    char candidate[1024];
    int n;

    char *base = SDL_GetBasePath();
    if (base) {
        n = snprintf(candidate, sizeof(candidate), "%s%s", base, filename);
        SDL_free(base);
        if (n > 0 && (size_t)n < sizeof(candidate)) {
            TTF_Font *f = TTF_OpenFont(candidate, size);
            if (f) return f;
        }
    }

    /* current working directory */
    n = snprintf(candidate, sizeof(candidate), "%s", filename);
    if (n > 0 && (size_t)n < sizeof(candidate)) {
        TTF_Font *f = TTF_OpenFont(candidate, size);
        if (f) return f;
    }

    /* common system locations (FIX-012: bounded, truncation-checked) */
    static const char *const dirs[] = {
        "/usr/share/fonts/truetype/dejavu/",
        "/usr/share/fonts/TTF/",
        "/opt/homebrew/share/fonts/",
        "/Library/Fonts/",
        "/System/Library/Fonts/Supplemental/",
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        n = snprintf(candidate, sizeof(candidate), "%s%s", dirs[i], filename);
        if (n <= 0 || (size_t)n >= sizeof(candidate)) continue;  /* truncated: skip */
        TTF_Font *f = TTF_OpenFont(candidate, size);
        if (f) return f;
    }
    return NULL;
}

/* Get a font for (size, bold, italic). Bold and italic are synthesized via
   SDL_ttf style flags from the regular face, so we do not depend on shipping
   dedicated bold/italic TTFs (FIX-013). A real bold/italic face would be
   preferred if present, but synthesis is sufficient for a reading browser and
   removes the missing-font failure mode. */
static TTF_Font *get_font(int size, int bold, int italic)
{
    bold = bold ? 1 : 0;
    italic = italic ? 1 : 0;

    for (int i = 0; i < font_cache_count; i++) {
        if (font_cache[i].size == size && font_cache[i].bold == bold &&
            font_cache[i].italic == italic)
            return font_cache[i].font;
    }

    if (font_cache_count >= FONT_CACHE_MAX) {
        /* Evict last entry */
        TTF_CloseFont(font_cache[FONT_CACHE_MAX - 1].font);
        font_cache_count = FONT_CACHE_MAX - 1;
    }

    TTF_Font *f = load_font_path("DejaVuSans.ttf", size);
    if (!f) f = load_font_path("DejaVuSans-Bold.ttf", size);  /* last resort */
    if (!f) f = load_font_path("LiberationSans-Regular.ttf", size);
    if (!f) return NULL;

    int style = TTF_STYLE_NORMAL;
    if (bold)   style |= TTF_STYLE_BOLD;
    if (italic) style |= TTF_STYLE_ITALIC;
    TTF_SetFontStyle(f, style);

    font_cache[font_cache_count].size = size;
    font_cache[font_cache_count].bold = bold;
    font_cache[font_cache_count].italic = italic;
    font_cache[font_cache_count].font = f;
    font_cache_count++;
    return f;
}

static void font_cache_clear(void)
{
    for (int i = 0; i < font_cache_count; i++) {
        if (font_cache[i].font) TTF_CloseFont(font_cache[i].font);
        font_cache[i].font = NULL;
    }
    font_cache_count = 0;
}

/* FontProvider bridge (FIX-008): measure text at the actual size/style using
   the render font cache, so layout wrapping matches what is painted. */
static int render_measure_text(void *ctx, const char *text,
                               int px, int bold, int italic)
{
    (void)ctx;
    if (!text || !*text || px <= 0) return 0;
    TTF_Font *f = get_font(px, bold, italic);   /* style baked into the entry */
    if (!f) return -1;   /* signal: fall back to approximation */
    int w = 0;
    if (TTF_SizeUTF8(f, text, &w, NULL) != 0) w = -1;
    return w;
}

static const FontProvider g_font_provider = { NULL, render_measure_text };

// ---------------------------------------------------------------------------
//     TEXTURE CACHE  (keyed by text pointer + font_size)
// ---------------------------------------------------------------------------
#define TCACHE_BUCKETS 512

typedef struct TCacheEntry {
    const char       *key;       /* text pointer (not owned) */
    int               font_size; /* font size used for rendering */
    int               bold;      /* bold variant */
    int               italic;    /* italic variant */
    SDL_Texture      *tex;
    int               w, h;
    struct TCacheEntry *next;
} TCacheEntry;

static TCacheEntry *tcache[TCACHE_BUCKETS];
static int tcache_count = 0;
#define TCACHE_MAX_ENTRIES 8192   /* FIX-011: bound texture-cache growth */

static void tcache_clear(void);  /* fwd */

static unsigned tcache_hash(const void *ptr, int font_size, int bold, int italic) {
    uintptr_t v = (uintptr_t)ptr;
    unsigned h = (unsigned)((v >> 4) ^ (v >> 16));
    h ^= (unsigned)font_size * 2654435761u;
    h ^= (unsigned)bold * 31;
    h ^= (unsigned)italic * 131;
    return h % TCACHE_BUCKETS;
}

static TCacheEntry *tcache_lookup(const char *key, int font_size, int bold, int italic) {
    for (TCacheEntry *e = tcache[tcache_hash(key, font_size, bold, italic)]; e; e = e->next)
        if (e->key == key && e->font_size == font_size &&
            e->bold == bold && e->italic == italic) return e;
    return NULL;
}

static void tcache_insert(const char *key, int font_size, int bold, int italic,
                           SDL_Texture *tex, int w, int h) {
    /* FIX-011: cap total entries; when exceeded, flush the whole cache. The
       textures are re-rasterized lazily on the next frame, so this keeps long
       sessions/huge pages bounded without a complex LRU. */
    if (tcache_count >= TCACHE_MAX_ENTRIES) tcache_clear();

    unsigned idx = tcache_hash(key, font_size, bold, italic);
    TCacheEntry *e = malloc(sizeof *e);
    if (!e) return;
    e->key  = key;
    e->font_size = font_size;
    e->bold = bold;
    e->italic = italic;
    e->tex  = tex;
    e->w    = w;
    e->h    = h;
    e->next = tcache[idx];
    tcache[idx] = e;
    tcache_count++;
}

static void tcache_clear(void) {
    for (int i = 0; i < TCACHE_BUCKETS; i++) {
        TCacheEntry *e = tcache[i];
        while (e) {
            TCacheEntry *next = e->next;
            if (e->tex) SDL_DestroyTexture(e->tex);
            free(e);
            e = next;
        }
        tcache[i] = NULL;
    }
    tcache_count = 0;
}

// ---------------------------------------------------------------------------
//     HISTORY
// ---------------------------------------------------------------------------
static void history_push(const char *url) {
    for (int i = history_pos + 1; i < history_count; i++) {
        free(history_urls[i]);
        history_urls[i] = NULL;
    }
    history_count = history_pos + 1;

    if (history_count >= HISTORY_MAX) {
        free(history_urls[0]);
        memmove(history_urls, history_urls + 1, (HISTORY_MAX - 1) * sizeof(char*));
        history_count = HISTORY_MAX - 1;
    }

    history_urls[history_count] = strdup(url);
    history_pos = history_count;
    history_count++;
}

static void history_free(void) {
    for (int i = 0; i < history_count; i++) {
        free(history_urls[i]);
        history_urls[i] = NULL;
    }
    history_count = 0;
    history_pos = -1;
}

// ---------------------------------------------------------------------------
//     UTILITY
// ---------------------------------------------------------------------------
static inline bool is_url(const char *s) {
    return s && strstr(s, "://");
}

static void build_target_url(const char *query, char *dst, size_t dst_sz) {
    if (is_url(query)) {
        snprintf(dst, dst_sz, "%s", query);
        return;
    }
    char encoded[SEARCH_BUFFER_SIZE] = {0};
    size_t j = 0;
    for (size_t i = 0; query[i] && j < sizeof(encoded) - 1; ++i) {
        encoded[j++] = (query[i] == ' ') ? '+' : query[i];
    }
    snprintf(dst, dst_sz, "https://www.google.com/m/search?q=%s", encoded);
}

static void resolve_url(const char *base, const char *href, char *dst, size_t dst_sz) {
    url_resolve(base, href, dst, dst_sz);   /* shared, tested (url.c) */
}

static SDL_Texture *create_text_texture(SDL_Renderer *r, TTF_Font *f,
                                        const char *txt, SDL_Color col,
                                        int *out_w, int *out_h)
{
    if (!txt || !*txt || !f) return NULL;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(f, txt, col);
    if (!surf) return NULL;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex && out_w && out_h) { *out_w = surf->w; *out_h = surf->h; }
    SDL_FreeSurface(surf);
    return tex;
}

static int calc_content_height(const Layout *layout) {
    int bottom = 0;
    for (size_t i = 0; i < layout->count; ++i) {
        const LayoutBox *b = &layout->boxes[i];
        int b_bottom = b->y + b->height;
        if (b_bottom > bottom) bottom = b_bottom;
    }
    return bottom;
}

static void clamp_scroll(void) {
    const int view_h = window_h - SEARCH_BAR_HEIGHT;
    int min_scroll = -(content_height - view_h);
    if (min_scroll > 0) min_scroll = 0;
    if (scroll_offset < min_scroll) scroll_offset = min_scroll;
    if (scroll_offset > 0)         scroll_offset = 0;
}

// ---------------------------------------------------------------------------
//     RENDER HELPERS
// ---------------------------------------------------------------------------
static int find_box_matches(const LayoutBox *b);  /* fwd (FEAT-008) */

static void render_search_bar(SDL_Renderer *ren) {
    TTF_Font *font = get_font(16, 0, 0);
    SDL_Rect bar = {0, 0, window_w, SEARCH_BAR_HEIGHT};
    SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
    SDL_RenderFillRect(ren, &bar);

    if (search_focused)
        SDL_SetRenderDrawColor(ren, 50, 100, 200, 255);
    else
        SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
    SDL_RenderDrawRect(ren, &bar);

    const char *display = *search_query ? search_query : current_url;
    SDL_Color col = *search_query ? (SDL_Color){0,0,0,255} : (SDL_Color){80,80,80,255};

    char findbuf[320];
    if (find_active) {
        snprintf(findbuf, sizeof findbuf, "Find: %s", find_query);
        display = findbuf;
        col = (SDL_Color){0,0,0,255};
    }

    if (display && *display && font) {
        int tw, th;
        SDL_Texture *tex = create_text_texture(ren, font, display, col, &tw, &th);
        if (tex) {
            SDL_Rect dst = {10, (SEARCH_BAR_HEIGHT - th)/2, tw, th};
            SDL_RenderCopy(ren, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
        }
    }
}

static void render_content(SDL_Renderer *ren, const Layout *layout) {
    const Theme *theme = ui_theme(g_theme_index);
    SDL_Color text_color    = {theme->text.r, theme->text.g, theme->text.b, 255};
    SDL_Color link_color    = {theme->link.r, theme->link.g, theme->link.b, 255};
    SDL_Color heading_color = {theme->heading.r, theme->heading.g, theme->heading.b, 255};
    SDL_Color visited_color = {128, 60, 160, 255};   /* muted purple (FEAT-012) */

    for (size_t i = 0; i < layout->count; ++i) {
        const LayoutBox *b = &layout->boxes[i];
        const LayoutHints *h = &b->hints;
        SDL_Rect rect = {b->x, b->y + scroll_offset + SEARCH_BAR_HEIGHT, b->width, b->height};

        /* early clip: skip boxes outside viewport */
        if (rect.y >= window_h || rect.y + rect.h <= SEARCH_BAR_HEIGHT) continue;
        if (!b->node) continue;

        /* Find-in-page highlight (FEAT-008): draw behind matching text. */
        if (find_box_matches(b)) {
            SDL_SetRenderDrawColor(ren, 255, 235, 120, 255);
            SDL_RenderFillRect(ren, &rect);
        }

        /* ---- Block background fill (FEAT-006) ---- */
        if (b->height > 0 && b->node->style && b->node->style->background &&
            b->node->name && b->node->name[0] != '#') {
            Color bg;
            if (style_parse_color(b->node->style->background, &bg) && bg.a > 0) {
                SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, 255);
                SDL_RenderFillRect(ren, &rect);
            }
        }

        /* ---- Wireframe borders for structural elements ---- */
        if (h->show_border && b->height > 0) {
            SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
            SDL_RenderDrawRect(ren, &rect);
        }

        /* ---- Blockquote left bar ---- */
        if (h->is_blockquote) {
            SDL_SetRenderDrawColor(ren, 160, 160, 160, 255);
            SDL_Rect bar = {rect.x - 5, rect.y, 3, rect.h};
            SDL_RenderFillRect(ren, &bar);
        }

        /* ---- Pre/code background ---- */
        if (h->is_pre && b->height > 0) {
            SDL_SetRenderDrawColor(ren, 240, 238, 235, 255);
            SDL_RenderFillRect(ren, &rect);
            SDL_SetRenderDrawColor(ren, 210, 208, 205, 255);
            SDL_RenderDrawRect(ren, &rect);
        }

        /* ---- <hr> ---- */
        if (h->is_hr) {
            SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
            int mid_y = rect.y + rect.h / 2;
            SDL_Rect line = {rect.x, mid_y, rect.w, 1};
            SDL_RenderFillRect(ren, &line);
            continue;
        }

        /* ---- <img> placeholder (MISS-002): box + alt text ---- */
        if (h->is_image) {
            SDL_SetRenderDrawColor(ren, 235, 235, 235, 255);
            SDL_RenderFillRect(ren, &rect);
            SDL_SetRenderDrawColor(ren, 170, 170, 170, 255);
            SDL_RenderDrawRect(ren, &rect);
            const char *alt = b->node ? dom_attr(b->node, "alt") : NULL;
            if (alt && *alt) {
                TTF_Font *af = get_font(13, 0, 1);
                if (af) {
                    int tw, th;
                    SDL_Color c = {120,120,120,255};
                    SDL_Texture *tx = create_text_texture(ren, af, alt, c, &tw, &th);
                    if (tx) {
                        SDL_Rect d = {rect.x + 4, rect.y + (rect.h - th)/2, tw, th};
                        if (tw > rect.w - 8) d.w = rect.w - 8; else d.w = tw;
                        d.h = th;
                        SDL_RenderCopy(ren, tx, NULL, &d);
                        SDL_DestroyTexture(tx);
                    }
                }
            }
            continue;
        }

        /* ---- List markers ---- */
        if (h->is_list_item) {
            int fs = h->font_size > 0 ? h->font_size : 16;
            TTF_Font *mfont = get_font(fs, 0, 0);
            if (mfont) {
                char marker[16];
                if (h->list_index > 0)
                    snprintf(marker, sizeof(marker), "%d.", h->list_index);
                else
                    snprintf(marker, sizeof(marker), "\xe2\x80\xa2"); /* U+2022 bullet */

                int tw, th;
                SDL_Texture *tex = create_text_texture(ren, mfont, marker,
                    text_color, &tw, &th);
                if (tex) {
                    /* Right-align marker in its box */
                    int text_y = rect.y + (rect.h - th) / 2;
                    SDL_Rect dst = {rect.x + rect.w - tw - 4, text_y, tw, th};
                    SDL_RenderCopy(ren, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
            }
            continue;
        }

        /* ---- Text rendering ---- */
        if (b->node->name && strcmp(b->node->name, "#text") == 0 && b->node->text) {
            /* FEAT-011: render the box's word-run substring of node->text. */
            const char *run_start = b->node->text + b->run_off;
            int run_len = b->run_len;
            char runbuf[512];
            if (run_len > 0) {
                int n = run_len < (int)sizeof(runbuf) - 1 ? run_len : (int)sizeof(runbuf) - 1;
                memcpy(runbuf, run_start, (size_t)n);
                runbuf[n] = '\0';
            } else {
                /* whole-node fallback (non word-split text) */
                snprintf(runbuf, sizeof runbuf, "%s", b->node->text);
                run_start = b->node->text;
            }

            bool empty = true;
            for (const char *p = runbuf; *p; ++p) {
                if (!isspace((unsigned char)*p)) { empty = false; break; }
            }
            if (empty) continue;

            int fs = h->font_size > 0 ? h->font_size : 16;
            int bold = h->is_bold;
            int italic = h->is_italic;

            /* Visited-link underline color (FEAT-012). */
            SDL_Color ul_color = link_color;
            if (h->is_link && b->href) {
                char resolved[2048];
                resolve_url(current_url, b->href, resolved, sizeof resolved);
                if (ui_visited_contains(resolved)) ul_color = visited_color;
            }

            /* Cache key = run start pointer (unique per word within a page). */
            const char *key = run_start;
            TCacheEntry *cached = tcache_lookup(key, fs, bold, italic);
            if (cached) {
                int text_y = rect.y + (rect.h - cached->h) / 2;
                SDL_Rect dst = {rect.x, text_y, cached->w, cached->h};
                SDL_RenderCopy(ren, cached->tex, NULL, &dst);
                if (h->is_link) {
                    SDL_SetRenderDrawColor(ren, ul_color.r, ul_color.g, ul_color.b, 255);
                    SDL_RenderDrawLine(ren, rect.x, text_y + cached->h,
                                       rect.x + cached->w, text_y + cached->h);
                }
            } else {
                SDL_Color col;
                if (h->is_link)
                    col = link_color;
                else if (h->is_heading)
                    col = heading_color;
                else
                    col = text_color;

                if (!h->is_link && b->node->style && b->node->style->color) {
                    Color c;
                    if (style_parse_color(b->node->style->color, &c)) {
                        col.r = c.r; col.g = c.g; col.b = c.b; col.a = 255;
                    }
                }

                TTF_Font *font = get_font(fs, bold, italic);
                if (!font) continue;

                int tw, th;
                SDL_Texture *tex = create_text_texture(ren, font, runbuf, col, &tw, &th);
                if (tex) {
                    tcache_insert(key, fs, bold, italic, tex, tw, th);
                    int text_y = rect.y + (rect.h - th) / 2;
                    SDL_Rect dst = {rect.x, text_y, tw, th};
                    SDL_RenderCopy(ren, tex, NULL, &dst);
                    if (h->is_link) {
                        SDL_SetRenderDrawColor(ren, ul_color.r, ul_color.g, ul_color.b, 255);
                        SDL_RenderDrawLine(ren, rect.x, text_y + th,
                                           rect.x + tw, text_y + th);
                    }
                    /* texture owned by cache */
                }
            }
        }
    }
}

/* Find-in-page (FEAT-008): is this text box a match for the find query? */
static int find_box_matches(const LayoutBox *b) {
    if (!find_active || !*find_query) return 0;
    if (!b->node || !b->node->name || strcmp(b->node->name, "#text") != 0) return 0;
    if (!b->node->text) return 0;
    return ui_str_contains_ci(b->node->text, find_query);
}

/* Count matches and return the index of the nth match (or -1). */
static int find_count_matches(const Layout *lay) {
    if (!lay) return 0;
    int n = 0;
    for (size_t i = 0; i < lay->count; i++)
        if (find_box_matches(&lay->boxes[i])) n++;
    return n;
}

/* Scroll so the current match is centered. */
static void find_scroll_to_current(void) {
    if (!currentLayout || !find_active) return;
    int n = 0;
    for (size_t i = 0; i < currentLayout->count; i++) {
        if (find_box_matches(&currentLayout->boxes[i])) {
            if (n == find_current) {
                const LayoutBox *b = &currentLayout->boxes[i];
                int view_h = window_h - SEARCH_BAR_HEIGHT;
                scroll_offset = ui_scroll_to_center(b->y, b->height, view_h, content_height);
                clamp_scroll();
                return;
            }
            n++;
        }
    }
}

/* Draw the scrollbar thumb on the right edge (FEAT-007). */
static void render_scrollbar(SDL_Renderer *ren) {
    const int track_y = SEARCH_BAR_HEIGHT;
    const int track_h = window_h - SEARCH_BAR_HEIGHT;
    int view_h = track_h;
    int ty, th;
    if (!ui_scrollbar_thumb(content_height, view_h, scroll_offset,
                            track_y, track_h, &ty, &th))
        return;   /* content fits; no scrollbar */

    const Theme *theme = ui_theme(g_theme_index);
    const int bar_w = 8;
    SDL_Rect thumb = { window_w - bar_w, ty, bar_w, th };
    SDL_SetRenderDrawColor(ren, theme->scrollbar.r, theme->scrollbar.g,
                           theme->scrollbar.b, 255);
    SDL_RenderFillRect(ren, &thumb);
}

// ---------------------------------------------------------------------------
//     PAGE LOAD
// ---------------------------------------------------------------------------
static DOMNode *make_error_dom(const char *url) {
    DOMNode *root = create_dom_node("root", NULL);
    DOMNode *body = create_dom_node("body", NULL);
    DOMNode *h1   = create_dom_node("h1", NULL);
    DOMNode *h1t  = create_dom_node("#text", "Failed to load page");
    DOMNode *p    = create_dom_node("p", NULL);
    char msg[2048];
    snprintf(msg, sizeof(msg), "Could not fetch: %s", url);
    DOMNode *pt   = create_dom_node("#text", msg);
    DOMNode *p2   = create_dom_node("p", NULL);
    DOMNode *p2t  = create_dom_node("#text", "Check the URL and try again.");

    add_child(h1, h1t);
    add_child(p, pt);
    add_child(p2, p2t);
    add_child(body, h1);
    add_child(body, p);
    add_child(body, p2);
    add_child(root, body);
    return root;
}

static Layout *reload_page(const char *url) {
    printf("Loading: %s\n", url);
    FetchResult fr;
    DOMNode *dom = NULL;

    if (!fetch_into(url, &fr)) {
        fprintf(stderr, "fetch failed: %s\n", url);
        dom = make_error_dom(url);
    } else if (!fr.ok) {
        fprintf(stderr, "HTTP %ld: %s\n", fr.status, url);
        dom = make_error_dom(url);
        fetch_result_free(&fr);
    } else {
        /* Shared pipeline (FEAT-001): parse + CSS + scripts + text split.
           document_from_html owns the DOM via its Document; detach it so the
           Layout can take ownership (matches the existing render lifetime). */
        Document *doc = document_from_bytes(fr.body, fr.length, fr.final_url, fr.charset);
        fetch_result_free(&fr);
        if (doc) {
            dom = doc->root;
            doc->root = NULL;     /* transfer ownership to the layout */
            /* capture the title for the window/chrome before freeing the doc */
            snprintf(current_title, sizeof current_title, "%s",
                     doc->title ? doc->title : "");
            document_free(doc);
        }
        if (!dom) {
            fprintf(stderr, "parse failed: %s\n", url);
            dom = make_error_dom(url);
        }
    }

    /* Measure at true font sizes via the provider (FIX-008). */
    Layout *lo = layout_dom(dom, &g_font_provider, window_w);
    return lo;
}

/* Adopt a freshly built layout: free the previous layout and its DOM, then
   take ownership of the new layout's DOM (FIX-009). */
static void swap_to_layout(Layout *nl) {
    tcache_clear();
    DOMNode *new_dom = nl ? nl->dom : NULL;
    if (currentLayout) free_layout(currentLayout);
    if (currentDom && currentDom != new_dom) free_dom(currentDom);
    currentLayout = nl;
    currentDom = new_dom;

    /* Update the window title bar from the page <title> (FEAT-007). */
    if (g_window) {
        char title[600];
        if (*current_title)
            snprintf(title, sizeof title, "%s — xs", current_title);
        else
            snprintf(title, sizeof title, "xs");
        SDL_SetWindowTitle(g_window, title);
    }
}

static void navigate_to(const char *url) {
    Layout *nl = reload_page(url);
    if (nl) {
        swap_to_layout(nl);
        content_height = calc_content_height(nl);
        scroll_offset = 0;
        snprintf(current_url, sizeof(current_url), "%s", url);
        history_push(url);
        ui_visited_add(url);          /* FEAT-012 */
        *search_query = '\0';
        needs_redraw = true;
    }
}

/* If `node` is a submit control in a form, build the GET URL and navigate
   (MISS-004). Returns 1 if a submission happened. */
static int try_submit_form(DOMNode *node) {
    if (!node || !node->name) return 0;
    int is_submit = 0;
    if (strcmp(node->name, "button") == 0) {
        const char *t = dom_attr(node, "type");
        is_submit = (!t || strcmp(t, "submit") == 0);
    } else if (strcmp(node->name, "input") == 0) {
        const char *t = dom_attr(node, "type");
        is_submit = (t && strcmp(t, "submit") == 0);
    }
    if (!is_submit) return 0;

    DOMNode *form = form_owner(node);
    if (!form) return 0;

    const char *method = dom_attr(form, "method");
    if (method && strcasecmp(method, "post") == 0) {
        /* POST not supported yet (documented); ignore. */
        fprintf(stderr, "form: POST not supported\n");
        return 0;
    }

    const char *action = dom_attr(form, "action");
    const char *names[64]; const char *values[64];
    int n = form_collect_fields(form, names, values, 64);
    char query[2048];
    form_build_query(names, values, n, query, sizeof query);

    char base[2048];
    resolve_url(current_url, action ? action : current_url, base, sizeof base);
    /* strip any existing query on the action, then append ours */
    char *qm = strchr(base, '?');
    if (qm) *qm = '\0';
    char target[2600];
    snprintf(target, sizeof target, "%s?%s", base, query);
    navigate_to(target);
    return 1;
}

// ---------------------------------------------------------------------------
//     EVENT HANDLING
// ---------------------------------------------------------------------------
static void handle_event(SDL_Event *e, bool *running) {
    switch (e->type) {
    case SDL_QUIT:
        *running = false;
        break;

    case SDL_WINDOWEVENT:
        if (e->window.event == SDL_WINDOWEVENT_RESIZED ||
            e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            window_w = e->window.data1;
            window_h = e->window.data2;
            if (currentDom) {
                /* Relayout from the SAME DOM; free only the boxes (FIX-009). */
                free_layout(currentLayout);
                currentLayout = layout_dom(currentDom, &g_font_provider, window_w);
                content_height = calc_content_height(currentLayout);
                clamp_scroll();
                tcache_clear();
            }
            needs_redraw = true;
        }
        if (e->window.event == SDL_WINDOWEVENT_EXPOSED)
            needs_redraw = true;
        break;

    case SDL_MOUSEWHEEL:
        scroll_offset += e->wheel.y * SCROLL_STEP;
        clamp_scroll();
        needs_redraw = true;
        break;

    case SDL_MOUSEBUTTONUP:
        if (e->button.button == SDL_BUTTON_LEFT) {
            int mx = e->button.x;
            int my = e->button.y;

            if (my < SEARCH_BAR_HEIGHT) {
                search_focused = true;
                SDL_StartTextInput();
                needs_redraw = true;
            } else {
                search_focused = false;
                SDL_StopTextInput();
                needs_redraw = true;

                if (currentLayout) {
                    int handled = 0;
                    for (size_t i = 0; i < currentLayout->count && !handled; i++) {
                        LayoutBox *b = &currentLayout->boxes[i];
                        int bx = b->x;
                        int by = b->y + scroll_offset + SEARCH_BAR_HEIGHT;
                        if (!(mx >= bx && mx <= bx + b->width &&
                              my >= by && my <= by + b->height))
                            continue;

                        /* Link click (FEAT) */
                        if (b->href) {
                            char resolved[2048];
                            resolve_url(current_url, b->href, resolved, sizeof(resolved));
                            navigate_to(resolved);
                            handled = 1;
                            break;
                        }
                        /* Form submit click (MISS-004): check the box node and
                           its parent (text inside a button). */
                        if (b->node) {
                            if (try_submit_form(b->node) ||
                                (b->node->parent && try_submit_form(b->node->parent))) {
                                handled = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
        break;

    case SDL_KEYDOWN: {
        SDL_Keymod mod = SDL_GetModState();

        if (mod & KMOD_ALT) {
            if (e->key.keysym.sym == SDLK_LEFT && history_pos > 0) {
                history_pos--;
                Layout *nl = reload_page(history_urls[history_pos]);
                if (nl) {
                    swap_to_layout(nl);
                    content_height = calc_content_height(nl);
                    scroll_offset = 0;
                    snprintf(current_url, sizeof(current_url), "%s", history_urls[history_pos]);
                    *search_query = '\0';
                    needs_redraw = true;
                }
                break;
            }
            if (e->key.keysym.sym == SDLK_RIGHT && history_pos < history_count - 1) {
                history_pos++;
                Layout *nl = reload_page(history_urls[history_pos]);
                if (nl) {
                    swap_to_layout(nl);
                    content_height = calc_content_height(nl);
                    scroll_offset = 0;
                    snprintf(current_url, sizeof(current_url), "%s", history_urls[history_pos]);
                    *search_query = '\0';
                    needs_redraw = true;
                }
                break;
            }
        }

        /* Ctrl/Cmd+F toggles find-in-page (FEAT-008). */
        if ((mod & (KMOD_CTRL | KMOD_GUI)) && e->key.keysym.sym == SDLK_f) {
            find_active = true;
            search_focused = false;
            find_query[0] = '\0';
            find_current = 0;
            SDL_StartTextInput();
            needs_redraw = true;
            break;
        }

        /* Text zoom (MISS-009): Ctrl/Cmd +/-/0, relayout from current DOM. */
        if (mod & (KMOD_CTRL | KMOD_GUI)) {
            int z = layout_get_zoom();
            int changed = 0;
            if (e->key.keysym.sym == SDLK_EQUALS || e->key.keysym.sym == SDLK_PLUS ||
                e->key.keysym.sym == SDLK_KP_PLUS) { layout_set_zoom(z + 10); changed = 1; }
            else if (e->key.keysym.sym == SDLK_MINUS ||
                     e->key.keysym.sym == SDLK_KP_MINUS) { layout_set_zoom(z - 10); changed = 1; }
            else if (e->key.keysym.sym == SDLK_0) { layout_set_zoom(100); changed = 1; }
            if (changed) {
                if (currentDom) {
                    free_layout(currentLayout);
                    currentLayout = layout_dom(currentDom, &g_font_provider, window_w);
                    content_height = calc_content_height(currentLayout);
                    clamp_scroll();
                    tcache_clear();
                }
                needs_redraw = true;
                break;
            }
        }

        if (e->key.keysym.sym == SDLK_ESCAPE) {
            if (find_active) {
                find_active = false;
                find_query[0] = '\0';
                SDL_StopTextInput();
                needs_redraw = true;
                break;
            }
            search_focused = false;
            SDL_StopTextInput();
            *search_query = '\0';
            needs_redraw = true;
            break;
        }

        if (e->key.keysym.sym == SDLK_SLASH && !search_focused) {
            SDL_StopTextInput();
            search_focused = true;
            *search_query = '\0';
            needs_redraw = true;
            SDL_StartTextInput();
            break;
        }

        if (!search_focused && !find_active) {
            if (e->key.keysym.sym == SDLK_UP || e->key.keysym.sym == SDLK_k) {
                scroll_offset += SCROLL_STEP;
                clamp_scroll();
                needs_redraw = true;
            } else if (e->key.keysym.sym == SDLK_DOWN || e->key.keysym.sym == SDLK_j) {
                scroll_offset -= SCROLL_STEP;
                clamp_scroll();
                needs_redraw = true;
            } else if (e->key.keysym.sym == SDLK_t) {
                /* cycle theme (FEAT-007); clear texture cache so colors refresh */
                g_theme_index = (g_theme_index + 1) % ui_theme_count();
                tcache_clear();
                needs_redraw = true;
            } else if (e->key.keysym.sym == SDLK_r ||
                       ((SDL_GetModState() & KMOD_GUI) && e->key.keysym.sym == SDLK_r)) {
                /* reload (FEAT-009): re-fetch current URL, replace not push */
                if (*current_url) {
                    Layout *nl = reload_page(current_url);
                    if (nl) {
                        int keep = scroll_offset;
                        swap_to_layout(nl);
                        content_height = calc_content_height(nl);
                        scroll_offset = keep;
                        clamp_scroll();
                        needs_redraw = true;
                    }
                }
            }
        } else {
            if (e->key.keysym.sym == SDLK_UP) {
                scroll_offset += SCROLL_STEP;
                clamp_scroll();
                needs_redraw = true;
            } else if (e->key.keysym.sym == SDLK_DOWN) {
                scroll_offset -= SCROLL_STEP;
                clamp_scroll();
                needs_redraw = true;
            }
        }

        if (find_active && e->key.keysym.sym == SDLK_BACKSPACE && *find_query) {
            find_query[strlen(find_query) - 1] = '\0';
            find_current = 0;
            find_scroll_to_current();
            needs_redraw = true;
        } else if (search_focused && e->key.keysym.sym == SDLK_BACKSPACE && *search_query) {
            search_query[strlen(search_query) - 1] = '\0';
            needs_redraw = true;
        }

        if (e->key.keysym.sym == SDLK_RETURN) {
            if (find_active) {
                /* cycle to next match (FEAT-008) */
                int total = find_count_matches(currentLayout);
                if (total > 0) {
                    find_current = (find_current + 1) % total;
                    find_scroll_to_current();
                    needs_redraw = true;
                }
            } else if (*search_query) {
                char url[2048];
                build_target_url(search_query, url, sizeof url);
                navigate_to(url);
            }
        }
        break;
    }

    case SDL_TEXTINPUT:
        if (find_active) {
            if (strlen(find_query) + strlen(e->text.text) < sizeof(find_query) - 1) {
                strncat(find_query, e->text.text,
                        sizeof(find_query) - strlen(find_query) - 1);
                find_current = 0;
                find_scroll_to_current();
                needs_redraw = true;
            }
            break;
        }
        if (search_focused) {
            if (strlen(search_query) + strlen(e->text.text) < SEARCH_BUFFER_SIZE - 1) {
                strncat(search_query, e->text.text,
                        SEARCH_BUFFER_SIZE - strlen(search_query) - 1);
            }
            needs_redraw = true;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
//     MAIN ENTRY
// ---------------------------------------------------------------------------
void render_layout(DOMNode *dom, const char *initial_url) {
    currentDom = dom;   /* own the DOM from entry so all exits free it (FIX-009) */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "%s\n", SDL_GetError()); if (dom) free_dom(dom); currentDom = NULL; return; }
    if (TTF_Init() == -1)             { fprintf(stderr, "%s\n", TTF_GetError()); SDL_Quit(); if (dom) free_dom(dom); currentDom = NULL; return; }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Window *win = SDL_CreateWindow("xs",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_w, window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr, "%s\n", SDL_GetError()); goto quit_sdl; }
    g_window = win;

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "%s\n", SDL_GetError()); SDL_DestroyWindow(win); goto quit_sdl; }

    /* Initialize font cache with base font */
    TTF_Font *base_font = get_font(16, 0, 0);
    if (!base_font) {
        fprintf(stderr, "ERROR: Could not load any font\n");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        goto quit_sdl;
    }

    /* Initial layout */
    if (dom) {
        currentLayout = layout_dom(dom, &g_font_provider, window_w);
        if (currentLayout)
            content_height = calc_content_height(currentLayout);
    }
    if (initial_url) {
        snprintf(current_url, sizeof(current_url), "%s", initial_url);
        history_push(initial_url);
    }

    SDL_StartTextInput();
    bool running = true;

    while (running) {
        SDL_Event e;

        if (!needs_redraw) {
            if (!SDL_WaitEvent(&e)) continue;
            handle_event(&e, &running);
        }

        while (SDL_PollEvent(&e))
            handle_event(&e, &running);

        if (needs_redraw) {
            const Theme *theme = ui_theme(g_theme_index);
            SDL_SetRenderDrawColor(ren, theme->bg.r, theme->bg.g, theme->bg.b, 255);
            SDL_RenderClear(ren);
            render_search_bar(ren);
            if (currentLayout) render_content(ren, currentLayout);
            render_scrollbar(ren);
            SDL_RenderPresent(ren);
            needs_redraw = false;
        }
    }

    SDL_StopTextInput();
    tcache_clear();
    font_cache_clear();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    g_window = NULL;

quit_sdl:
    TTF_Quit();
    SDL_Quit();
    if (currentLayout) free_layout(currentLayout);
    if (currentDom) free_dom(currentDom);
    history_free();
    ui_visited_clear();
}
