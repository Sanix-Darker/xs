// Build: gcc -O2 -Wall -Wextra -pedantic xs_browser_optimized.c -lSDL2 -lSDL2_ttf -o xs
//-----------------------------------------------------------------------------
#include "render.h"
#include "layout.h"
#include "parser.h"
#include "css.h"
#include "network.h"
#include "javascript.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int  content_height        = 0;
static bool needs_redraw          = true;
static bool search_focused        = true;

/* Back/forward history */
static char *history_urls[HISTORY_MAX];
static int   history_count = 0;
static int   history_pos   = -1;

// ---------------------------------------------------------------------------
//     FONT CACHE  (size + bold -> TTF_Font*)
// ---------------------------------------------------------------------------
#define FONT_CACHE_MAX 16

typedef struct {
    int        size;
    int        bold;
    TTF_Font  *font;
} FontCacheEntry;

static FontCacheEntry font_cache[FONT_CACHE_MAX];
static int font_cache_count = 0;

static TTF_Font *load_font_path(const char *filename, int size)
{
    const char *paths[6];
    int n = 0;
    static char exe_font[1024];

    char *base = SDL_GetBasePath();
    if (base) {
        snprintf(exe_font, sizeof(exe_font), "%s%s", base, filename);
        SDL_free(base);
        paths[n++] = exe_font;
    }

    static char cwd_font[256];
    snprintf(cwd_font, sizeof(cwd_font), "%s", filename);
    paths[n++] = cwd_font;

    static char linux1[256], linux2[256], linux3[256];
    snprintf(linux1, sizeof(linux1), "/usr/share/fonts/truetype/dejavu/%s", filename);
    snprintf(linux2, sizeof(linux2), "/usr/share/fonts/TTF/%s", filename);
    snprintf(linux3, sizeof(linux3), "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
    paths[n++] = linux1;
    paths[n++] = linux2;
    paths[n++] = linux3;

    for (int i = 0; i < n; i++) {
        TTF_Font *f = TTF_OpenFont(paths[i], size);
        if (f) return f;
    }
    return NULL;
}

static TTF_Font *get_font(int size, int bold)
{
    /* Lookup existing */
    for (int i = 0; i < font_cache_count; i++) {
        if (font_cache[i].size == size && font_cache[i].bold == bold)
            return font_cache[i].font;
    }

    /* Open new font */
    if (font_cache_count >= FONT_CACHE_MAX) {
        /* Evict last entry */
        TTF_CloseFont(font_cache[FONT_CACHE_MAX - 1].font);
        font_cache_count = FONT_CACHE_MAX - 1;
    }

    const char *filename = bold ? "DejaVuSans-Bold.ttf" : "DejaVuSans.ttf";
    TTF_Font *f = load_font_path(filename, size);
    if (!f) {
        /* Fallback: try the other variant */
        filename = bold ? "DejaVuSans.ttf" : "DejaVuSans-Bold.ttf";
        f = load_font_path(filename, size);
    }
    if (!f) return NULL;

    font_cache[font_cache_count].size = size;
    font_cache[font_cache_count].bold = bold;
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

// ---------------------------------------------------------------------------
//     TEXTURE CACHE  (keyed by text pointer + font_size)
// ---------------------------------------------------------------------------
#define TCACHE_BUCKETS 512

typedef struct TCacheEntry {
    const char       *key;       /* text pointer (not owned) */
    int               font_size; /* font size used for rendering */
    int               bold;      /* bold variant */
    SDL_Texture      *tex;
    int               w, h;
    struct TCacheEntry *next;
} TCacheEntry;

static TCacheEntry *tcache[TCACHE_BUCKETS];

static unsigned tcache_hash(const void *ptr, int font_size, int bold) {
    uintptr_t v = (uintptr_t)ptr;
    unsigned h = (unsigned)((v >> 4) ^ (v >> 16));
    h ^= (unsigned)font_size * 2654435761u;
    h ^= (unsigned)bold * 31;
    return h % TCACHE_BUCKETS;
}

static TCacheEntry *tcache_lookup(const char *key, int font_size, int bold) {
    for (TCacheEntry *e = tcache[tcache_hash(key, font_size, bold)]; e; e = e->next)
        if (e->key == key && e->font_size == font_size && e->bold == bold) return e;
    return NULL;
}

static void tcache_insert(const char *key, int font_size, int bold,
                           SDL_Texture *tex, int w, int h) {
    unsigned idx = tcache_hash(key, font_size, bold);
    TCacheEntry *e = malloc(sizeof *e);
    e->key  = key;
    e->font_size = font_size;
    e->bold = bold;
    e->tex  = tex;
    e->w    = w;
    e->h    = h;
    e->next = tcache[idx];
    tcache[idx] = e;
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
    if (!href || !*href) { dst[0] = '\0'; return; }
    if (strstr(href, "://")) {
        snprintf(dst, dst_sz, "%s", href);
    } else if (href[0] == '/') {
        const char *p = strstr(base, "://");
        if (p) {
            p += 3;
            const char *slash = strchr(p, '/');
            if (slash) {
                int origin_len = (int)(slash - base);
                snprintf(dst, dst_sz, "%.*s%s", origin_len, base, href);
            } else {
                snprintf(dst, dst_sz, "%s%s", base, href);
            }
        } else {
            snprintf(dst, dst_sz, "%s", href);
        }
    } else {
        const char *last_slash = strrchr(base, '/');
        const char *scheme_end = strstr(base, "://");
        if (scheme_end && last_slash > scheme_end + 2) {
            int dir_len = (int)(last_slash - base) + 1;
            snprintf(dst, dst_sz, "%.*s%s", dir_len, base, href);
        } else {
            snprintf(dst, dst_sz, "%s/%s", base, href);
        }
    }
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
static void render_search_bar(SDL_Renderer *ren) {
    TTF_Font *font = get_font(16, 0);
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
    SDL_Color text_color    = {30, 30, 30, 255};
    SDL_Color link_color    = {20, 70, 180, 255};
    SDL_Color heading_color = {15, 15, 15, 255};

    for (size_t i = 0; i < layout->count; ++i) {
        const LayoutBox *b = &layout->boxes[i];
        const LayoutHints *h = &b->hints;
        SDL_Rect rect = {b->x, b->y + scroll_offset + SEARCH_BAR_HEIGHT, b->width, b->height};

        /* early clip: skip boxes outside viewport */
        if (rect.y >= window_h || rect.y + rect.h <= SEARCH_BAR_HEIGHT) continue;
        if (!b->node) continue;

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

        /* ---- List markers ---- */
        if (h->is_list_item) {
            int fs = h->font_size > 0 ? h->font_size : 16;
            TTF_Font *mfont = get_font(fs, 0);
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
            const char *text = b->node->text;
            bool empty = true;
            for (const char *p = text; *p; ++p) {
                if (!isspace((unsigned char)*p)) { empty = false; break; }
            }
            if (empty) continue;

            int fs = h->font_size > 0 ? h->font_size : 16;
            int bold = h->is_bold;

            /* Check texture cache (keyed by pointer + size + bold) */
            TCacheEntry *cached = tcache_lookup(text, fs, bold);
            if (cached) {
                /* Vertically center text in line-height box */
                int text_y = rect.y + (rect.h - cached->h) / 2;
                SDL_Rect dst = {rect.x, text_y, cached->w, cached->h};
                SDL_RenderCopy(ren, cached->tex, NULL, &dst);

                /* Link underline */
                if (h->is_link) {
                    SDL_SetRenderDrawColor(ren, link_color.r, link_color.g, link_color.b, 255);
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

                TTF_Font *font = get_font(fs, bold);
                if (!font) continue;

                int tw, th;
                SDL_Texture *tex = create_text_texture(ren, font, text, col, &tw, &th);
                if (tex) {
                    tcache_insert(text, fs, bold, tex, tw, th);
                    int text_y = rect.y + (rect.h - th) / 2;
                    SDL_Rect dst = {rect.x, text_y, tw, th};
                    SDL_RenderCopy(ren, tex, NULL, &dst);

                    /* Link underline */
                    if (h->is_link) {
                        SDL_SetRenderDrawColor(ren, link_color.r, link_color.g, link_color.b, 255);
                        SDL_RenderDrawLine(ren, rect.x, text_y + th,
                                           rect.x + tw, text_y + th);
                    }
                    /* texture owned by cache */
                }
            }
        }
    }
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
    char *html = fetch_url(url);
    DOMNode *dom = NULL;

    if (!html) {
        fprintf(stderr, "fetch failed: %s\n", url);
        dom = make_error_dom(url);
    } else {
        dom = parse_html(html);
        free(html);
        if (!dom) {
            fprintf(stderr, "parse failed: %s\n", url);
            dom = make_error_dom(url);
        }
    }

    split_text_nodes(dom);

    char *style_text = extract_style_text(dom);
    if (style_text) {
        CSSStyleSheet *sheet = parse_css(style_text);
        if (sheet) {
            apply_stylesheet_to_dom(sheet, dom);
            free_stylesheet(sheet);
        }
        free(style_text);
    }

    run_scripts_in_dom(dom);

    /* Use base font for measurement */
    TTF_Font *mfont = get_font(16, 0);
    Layout *lo = layout_dom(dom, mfont, window_w);
    return lo;
}

static void navigate_to(const char *url) {
    Layout *nl = reload_page(url);
    if (nl) {
        tcache_clear();
        if (currentLayout) free_layout(currentLayout);
        currentLayout = nl;
        content_height = calc_content_height(nl);
        scroll_offset = 0;
        snprintf(current_url, sizeof(current_url), "%s", url);
        history_push(url);
        *search_query = '\0';
        needs_redraw = true;
    }
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
            if (currentLayout && currentLayout->dom) {
                DOMNode *dom_ref = currentLayout->dom;
                currentLayout->dom = NULL;
                free_layout(currentLayout);
                TTF_Font *mfont = get_font(16, 0);
                currentLayout = layout_dom(dom_ref, mfont, window_w);
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
                    for (size_t i = 0; i < currentLayout->count; i++) {
                        LayoutBox *b = &currentLayout->boxes[i];
                        if (!b->href) continue;
                        int bx = b->x;
                        int by = b->y + scroll_offset + SEARCH_BAR_HEIGHT;
                        if (mx >= bx && mx <= bx + b->width &&
                            my >= by && my <= by + b->height) {
                            char resolved[2048];
                            resolve_url(current_url, b->href, resolved, sizeof(resolved));
                            navigate_to(resolved);
                            break;
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
                    tcache_clear();
                    if (currentLayout) free_layout(currentLayout);
                    currentLayout = nl;
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
                    tcache_clear();
                    if (currentLayout) free_layout(currentLayout);
                    currentLayout = nl;
                    content_height = calc_content_height(nl);
                    scroll_offset = 0;
                    snprintf(current_url, sizeof(current_url), "%s", history_urls[history_pos]);
                    *search_query = '\0';
                    needs_redraw = true;
                }
                break;
            }
        }

        if (e->key.keysym.sym == SDLK_ESCAPE) {
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

        if (!search_focused) {
            if (e->key.keysym.sym == SDLK_UP || e->key.keysym.sym == SDLK_k) {
                scroll_offset += SCROLL_STEP;
                clamp_scroll();
                needs_redraw = true;
            } else if (e->key.keysym.sym == SDLK_DOWN || e->key.keysym.sym == SDLK_j) {
                scroll_offset -= SCROLL_STEP;
                clamp_scroll();
                needs_redraw = true;
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

        if (search_focused && e->key.keysym.sym == SDLK_BACKSPACE && *search_query) {
            search_query[strlen(search_query) - 1] = '\0';
            needs_redraw = true;
        }

        if (e->key.keysym.sym == SDLK_RETURN && *search_query) {
            char url[2048];
            build_target_url(search_query, url, sizeof url);
            navigate_to(url);
        }
        break;
    }

    case SDL_TEXTINPUT:
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
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "%s\n", SDL_GetError()); return; }
    if (TTF_Init() == -1)             { fprintf(stderr, "%s\n", TTF_GetError()); SDL_Quit(); return; }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Window *win = SDL_CreateWindow("xs",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_w, window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr, "%s\n", SDL_GetError()); goto quit_sdl; }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "%s\n", SDL_GetError()); SDL_DestroyWindow(win); goto quit_sdl; }

    /* Initialize font cache with base font */
    TTF_Font *base_font = get_font(16, 0);
    if (!base_font) {
        fprintf(stderr, "ERROR: Could not load any font\n");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        goto quit_sdl;
    }

    /* Initial layout */
    if (dom) {
        currentLayout = layout_dom(dom, base_font, window_w);
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
            SDL_SetRenderDrawColor(ren, BG_R, BG_G, BG_B, 255);
            SDL_RenderClear(ren);
            render_search_bar(ren);
            if (currentLayout) render_content(ren, currentLayout);
            SDL_RenderPresent(ren);
            needs_redraw = false;
        }
    }

    SDL_StopTextInput();
    tcache_clear();
    font_cache_clear();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

quit_sdl:
    TTF_Quit();
    SDL_Quit();
    if (currentLayout) free_layout(currentLayout);
    history_free();
}
