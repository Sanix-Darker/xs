// Build: gcc -O2 -Wall -Wextra -pedantic xs_browser_optimized.c -lSDL2 -lSDL2_ttf -o xs
//-----------------------------------------------------------------------------
#include "render.h"
#include "layout.h"
#include "parser.h"
#include "css.h"      // For computed style access.
#include "network.h"  // For fetch_url(), etc.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
//     CONFIG & GLOBALS
// ---------------------------------------------------------------------------
#define WINDOW_WIDTH          950
#define WINDOW_HEIGHT         700
#define SEARCH_BAR_HEIGHT     40
#define SEARCH_BUFFER_SIZE    1024
#define SCROLL_STEP           20

static int  scroll_offset         = 0;            // Positive = scrolling up.
static char search_query[SEARCH_BUFFER_SIZE] = "";
static Layout *currentLayout      = NULL;
static int  content_height        = 0;            // Calculated when layout changes.

// ---------------------------------------------------------------------------
//     UTILITY
// ---------------------------------------------------------------------------
static inline bool is_url(const char *s) {
    return s && strstr(s, "://"); // yes laziness at it's peak !!!!
}

static void build_target_url(const char *query, char *dst, size_t dst_sz) {
    if (is_url(query)) {
        snprintf(dst, dst_sz, "%s", query);
        return;
    }
    // naive encoding: space -> '+'
    // because am too lazy to implement urlEncode in C
    //
    // yes
    char encoded[SEARCH_BUFFER_SIZE] = {0};
    size_t j = 0;
    for (size_t i = 0; query[i] && j < sizeof(encoded) - 1; ++i) {
        encoded[j++] = (query[i] == ' ') ? '+' : query[i];
    }
    snprintf(dst, dst_sz, "https://www.google.com/m/search?q=%s", encoded);
}

static void parse_hex_color(const char *hex, Uint8 *r, Uint8 *g, Uint8 *b) {
    *r = *g = *b = 0;
    if (!hex || hex[0] != '#' || (strlen(hex) != 7 && strlen(hex) != 4)) return;

    if (strlen(hex) == 7) {
        *r = (Uint8) strtol((char[]){hex[1], hex[2], 0}, NULL, 16);
        *g = (Uint8) strtol((char[]){hex[3], hex[4], 0}, NULL, 16);
        *b = (Uint8) strtol((char[]){hex[5], hex[6], 0}, NULL, 16);
    } else { // #RGB shorthand
        *r = (Uint8) strtol((char[]){hex[1], hex[1], 0}, NULL, 16);
        *g = (Uint8) strtol((char[]){hex[2], hex[2], 0}, NULL, 16);
        *b = (Uint8) strtol((char[]){hex[3], hex[3], 0}, NULL, 16);
    }
}

static SDL_Texture *create_text_texture(SDL_Renderer *r, TTF_Font *f,
                                        const char *txt, int *out_w, int *out_h)
{
    if (!txt || !*txt) return NULL;
    SDL_Color col = {0, 0, 0, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(f, txt, col);
    if (!surf) return NULL;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex && out_w && out_h) { *out_w = surf->w; *out_h = surf->h; }
    // NOTE: turns out, on old SLD, this was not needed
    // painfull for nothing special
    SDL_FreeSurface(surf);
    return tex;
}

static int calc_content_height(const Layout *layout) {
    int bottom = 0;
    for (int i = 0; i < layout->count; ++i) {
        // AAAAAAAAAHHHHHHHHHHHHH :-(
        const LayoutBox *b = &layout->boxes[i];
        bottom = (b->y + b->height > bottom) ? b->y + b->height : bottom;
    }
    return bottom;
}

static void clamp_scroll(void) {
    const int view_h = WINDOW_HEIGHT - SEARCH_BAR_HEIGHT;
    int min_scroll = -(content_height - view_h);
    if (min_scroll > 0) min_scroll = 0; // content fits entirely.
    if (scroll_offset < min_scroll)   scroll_offset = min_scroll;
    if (scroll_offset > 0)            scroll_offset = 0;
}

// ---------------------------------------------------------------------------
//     RENDER HELPERS
// ---------------------------------------------------------------------------
static void render_search_bar(SDL_Renderer *ren, TTF_Font *font) {
    SDL_Rect bar = {0, 0, WINDOW_WIDTH, SEARCH_BAR_HEIGHT};
    SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
    SDL_RenderFillRect(ren, &bar);
    SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
    SDL_RenderDrawRect(ren, &bar);

    int tw, th;
    SDL_Texture *tex = create_text_texture(ren, font, search_query, &tw, &th);
    if (tex) {
        SDL_Rect dst = {10, (SEARCH_BAR_HEIGHT - th)/2, tw, th};
        SDL_RenderCopy(ren, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
}

static void render_content(SDL_Renderer *ren, TTF_Font *font, const Layout *layout) {
    SDL_Rect view = {0, SEARCH_BAR_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT - SEARCH_BAR_HEIGHT};
    SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
    SDL_RenderDrawRect(ren, &view);

    for (int i = 0; i < layout->count; ++i) {
        const LayoutBox *b = &layout->boxes[i];
        SDL_Rect rect = {b->x, b->y + scroll_offset + SEARCH_BAR_HEIGHT, b->width, b->height};

        // early clip: skip boxes outside viewport
        if (rect.y >= WINDOW_HEIGHT || rect.y + rect.h <= SEARCH_BAR_HEIGHT) continue;

        // skip empty boxes quickly
        bool empty = true;
        if (b->node) {
            if (strcmp(b->node->name, "#text") == 0 && b->node->text) {
                const char *p = b->node->text;
                while (*p) { if (!isspace((unsigned char)*p)) { empty = false; break; } ++p; }
            } else if (b->node->children_count || (b->node->style && b->node->style->background)) {
                empty = false;
            }
        }
        if (empty) continue;

        // let's not draw rectangles
        //// background fill
        //if (b->node && b->node->style && b->node->style->background) {
        //    Uint8 r, g, bl;
        //    parse_hex_color(b->node->style->background, &r, &g, &bl);
        //    SDL_SetRenderDrawColor(ren, r, g, bl, 255);
        //    SDL_RenderFillRect(ren, &rect);
        //}
        //// outline
        //SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        //SDL_RenderDrawRect(ren, &rect);

        if (b->node && strcmp(b->node->name, "#text") == 0 && b->node->text) {
            int tw, th;
            SDL_Texture *tex = create_text_texture(ren, font, b->node->text, &tw, &th);
            if (tex) {
                SDL_Rect dst = {rect.x + 5, rect.y + 5, tw, th};
                SDL_RenderCopy(ren, tex, NULL, &dst);
                SDL_DestroyTexture(tex);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//     PAGE LOAD
// ---------------------------------------------------------------------------
static Layout *reload_page(const char *url) {
    printf("Loading: %s\n", url);
    char *html = fetch_url(url);
    if (!html) { fprintf(stderr, "fetch failed!\n"); return NULL; }
    DOMNode *dom = parse_html(html);
    free(html);
    if (!dom) { fprintf(stderr, "parse failed!\n"); return NULL; }
    Layout *lo = layout_dom(dom);
    lo->dom = dom; // transfer ownership
    return lo;
}

// ---------------------------------------------------------------------------
//     MAIN ENTRY
// ---------------------------------------------------------------------------
void render_layout(Layout *initial) {
    currentLayout = initial;
    if (currentLayout) content_height = calc_content_height(currentLayout);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "%s\n", SDL_GetError()); return; }
    if (TTF_Init() == -1)           { fprintf(stderr, "%s\n", TTF_GetError()); SDL_Quit(); return; }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Window   *win = SDL_CreateWindow("xs", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "%s\n", SDL_GetError()); goto quit_sdl; }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "%s\n", SDL_GetError()); SDL_DestroyWindow(win); goto quit_sdl; }

    char *base = SDL_GetBasePath();
    char font_p[1024];
    snprintf(font_p, sizeof(font_p), "%sDejaVuSans.ttf", base ? base : "");
    if (base) SDL_free(base);

    TTF_Font *font = TTF_OpenFont(font_p, 16);
    if (!font) { fprintf(stderr, "%s\n", TTF_GetError()); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); goto quit_sdl; }

    SDL_StartTextInput();
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_MOUSEWHEEL:
                    scroll_offset += e.wheel.y * SCROLL_STEP;
                    clamp_scroll();
                    break;
                case SDL_KEYDOWN:
                    if (e.key.keysym.sym == SDLK_UP  || e.key.keysym.sym == SDLK_k) { scroll_offset += SCROLL_STEP; clamp_scroll(); }
                    else if (e.key.keysym.sym == SDLK_DOWN || e.key.keysym.sym == SDLK_j) { scroll_offset -= SCROLL_STEP; clamp_scroll(); }
                    else if (e.key.keysym.sym == SDLK_BACKSPACE && *search_query) {
                        search_query[strlen(search_query) - 1] = '\0';
                    }
                    else if (e.key.keysym.sym == SDLK_RETURN) {
                        char url[2048];
                        build_target_url(search_query, url, sizeof url);
                        Layout *nl = reload_page(url);
                        if (nl) {
                            if (currentLayout) free_layout(currentLayout);
                            currentLayout = nl;
                            content_height = calc_content_height(nl);
                            scroll_offset = 0;
                            *search_query = '\0';
                        }
                    }
                    break;
                case SDL_TEXTINPUT:
                    if (strlen(search_query) + strlen(e.text.text) < SEARCH_BUFFER_SIZE - 1) {
                        strncat(search_query, e.text.text, SEARCH_BUFFER_SIZE - strlen(search_query) - 1);
                    }
                    break;
            }
        }

        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);
        render_search_bar(ren, font);
        if (currentLayout) render_content(ren, font, currentLayout);
        SDL_RenderPresent(ren);
    }

    SDL_StopTextInput();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

quit_sdl:
    TTF_Quit();
    SDL_Quit();
    if (currentLayout) free_layout(currentLayout);
}
