#include "render.h"
#include "layout.h"
#include "parser.h"
#include "css.h"      // For computed style access.
#include "network.h"  // For fetch_url(), etc.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Global variables for scrolling.
static int scroll_offset = 0;

// --- Search Bar Global State ---
#define SEARCH_BUFFER_SIZE 1024
static char search_query[SEARCH_BUFFER_SIZE] = "";
static const int SEARCH_BAR_HEIGHT = 40;

// Global pointer to the current layout.
static Layout *currentLayout = NULL;

// --- Helper: Determine if a string is a URL ---
// For simplicity, we consider it a URL if it contains "://".
static bool is_url(const char *str) {
    return (strstr(str, "://") != NULL);
}

// --- Helper: Build the target URL based on search_query ---
// If search_query looks like a URL, return it directly;
// otherwise, build a Google mobile search URL.
static void build_target_url(const char *query, char *target, size_t target_size) {
    if (is_url(query)) {
        snprintf(target, target_size, "%s", query);
    } else {
        // In a full browser you'd URL-encode properly; here, we simply replace spaces.
        char encoded[SEARCH_BUFFER_SIZE];
        size_t j = 0;
        for (size_t i = 0; query[i] && j < sizeof(encoded) - 1; i++) {
            if (query[i] == ' ') {
                encoded[j++] = '+';
            } else {
                encoded[j++] = query[i];
            }
        }
        encoded[j] = '\0';
        snprintf(target, target_size, "https://www.google.com/m/search?q=%s", encoded);
    }
}

// --- Helper: Parse a hex color string (e.g. "#RRGGBB") into RGB values.
static void parse_hex_color(const char *hex, Uint8 *r, Uint8 *g, Uint8 *b) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) {
        *r = *g = *b = 0;
        return;
    }
    char rs[3] = { hex[1], hex[2], '\0' };
    char gs[3] = { hex[3], hex[4], '\0' };
    char bs[3] = { hex[5], hex[6], '\0' };
    *r = (Uint8)strtol(rs, NULL, 16);
    *g = (Uint8)strtol(gs, NULL, 16);
    *b = (Uint8)strtol(bs, NULL, 16);
}

// --- Helper: Render text using SDL_ttf.
// Added a check to avoid rendering an empty string.
static void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {
    if (!text || text[0] == '\0') {
        // Nothing to render.
        return;
    }
    SDL_Color textColor = {0, 0, 0, 255};  // black text
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, textColor);
    if (!textSurface) {
        //fprintf(stderr, "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        // fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(textSurface);
        return;
    }
    SDL_Rect renderQuad = { x, y, textSurface->w, textSurface->h };
    SDL_FreeSurface(textSurface);
    SDL_RenderCopy(renderer, textTexture, NULL, &renderQuad);
    SDL_DestroyTexture(textTexture);
}

// --- Render the search bar.
static void render_search_bar(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_Rect searchRect = { 0, 0, 800, SEARCH_BAR_HEIGHT };
    // Fill with a light background.
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &searchRect);
    // Draw a border.
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &searchRect);
    // Render the search query text inside the search bar.
    render_text(renderer, font, search_query, 10, 10);
}

// --- Render the content frame.
static void render_content(SDL_Renderer *renderer, TTF_Font *font, Layout* layout) {
    // Define the content area (below the search bar).
    SDL_Rect contentRect = { 0, SEARCH_BAR_HEIGHT, 800, 600 - SEARCH_BAR_HEIGHT };
    // Draw a border around the content area.
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &contentRect);

    // Render each layout box.
    for (int i = 0; i < layout->count; i++) {
        LayoutBox *box = &layout->boxes[i];

        // Check if the box should be considered empty.
        bool empty = false;
        if (box->node) {
            if (strcmp(box->node->name, "#text") == 0) {
                // For text nodes, if the text is empty or only whitespace, mark as empty.
                if (!box->node->text) {
                    empty = true;
                } else {
                    const char *p = box->node->text;
                    bool hasVisible = false;
                    while (*p) {
                        if (!isspace((unsigned char)*p)) { hasVisible = true; break; }
                        p++;
                    }
                    if (!hasVisible) {
                        empty = true;
                    }
                }
            } else {
                // For other nodes, consider them empty if they have no text,
                // no children, and no background (you can modify this logic as needed).
                int textLength = (box->node->text) ? strlen(box->node->text) : 0;
                if (box->node->children_count == 0 && textLength == 0 &&
                    (!box->node->style || !box->node->style->background)) {
                    empty = true;
                }
            }
        }

        if (empty) {
            continue; // Skip drawing this empty box.
        }

        // Adjust Y coordinate by scroll_offset and search bar height.
        SDL_Rect rect = { box->x, box->y + scroll_offset + SEARCH_BAR_HEIGHT, box->width, box->height };

        // Fill rectangle if a background color is specified.
        if (box->node && box->node->style && box->node->style->background) {
            Uint8 r, g, b;
            parse_hex_color(box->node->style->background, &r, &g, &b);
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            SDL_RenderFillRect(renderer, &rect);
        }

        // Draw the rectangle outline.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &rect);

        // Render text for text nodes.
        if (box->node && strcmp(box->node->name, "#text") == 0 && box->node->text) {
            render_text(renderer, font, box->node->text, rect.x + 5, rect.y + 5);
        }
    }
}

// --- Reload page: fetch, parse, and layout the new page.
static Layout* reload_page(const char *target_url) {
    printf("Loading URL: %s\n", target_url);
    char *html = fetch_url(target_url);
    if (!html) {
        fprintf(stderr, "Failed to fetch URL: %s\n", target_url);
        return NULL;
    }
    DOMNode *dom = parse_html(html);
    free(html);
    if (!dom) {
        fprintf(stderr, "Failed to parse HTML from URL: %s\n", target_url);
        return NULL;
    }
    Layout *newLayout = layout_dom(dom);
    // Instead of freeing the DOM here, store it in the layout:
    newLayout->dom = dom;
    return newLayout;
}

// --- Main render_layout function.
void render_layout(Layout* initialLayout) {
    // Use the initial layout if provided.
    currentLayout = initialLayout;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        SDL_Quit();
        return;
    }
    // Create a window with default flags (no resizable/native decorations here).
    SDL_Window *window = SDL_CreateWindow("xs Browser",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          800, 600, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return;
    }
    // Load a font.
    char *base_path = SDL_GetBasePath();
    char font_path[1024];
    if (base_path) {
        snprintf(font_path, sizeof(font_path), "%sDejaVuSans.ttf", base_path);
        SDL_free(base_path);
    } else {
        strncpy(font_path, "DejaVuSans.ttf", sizeof(font_path));
    }
    TTF_Font *font = TTF_OpenFont(font_path, 16);
    if (!font) {
        fprintf(stderr, "Failed to load font from '%s'! SDL_ttf Error: %s\n", font_path, TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return;
    }

    // Enable text input for the search bar.
    SDL_StartTextInput();

    bool running = true;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                scroll_offset += e.wheel.y * 20;
            }
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    // Vim and arrow keys for vertical scrolling.
                    case SDLK_UP:
                    case SDLK_k:
                        scroll_offset += 20;
                        break;
                    case SDLK_DOWN:
                    case SDLK_j:
                        scroll_offset -= 20;
                        break;
                    // Backspace for editing search query.
                    case SDLK_BACKSPACE:
                        {
                            int len = strlen(search_query);
                            if (len > 0) {
                                search_query[len - 1] = '\0';
                            }
                        }
                        break;
                    // On ENTER, reload the page.
                    case SDLK_RETURN:
                        {
                            char target_url[2048];
                            build_target_url(search_query, target_url, sizeof(target_url));
                            Layout *newLayout = reload_page(target_url);
                            if (newLayout) {
                                if (currentLayout) {
                                    free_layout(currentLayout);
                                }
                                currentLayout = newLayout;
                                scroll_offset = 0;
                                search_query[0] = '\0';
                            } else {
                                fprintf(stderr, "Failed to load new page.\n");
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            else if (e.type == SDL_TEXTINPUT) {
                if (strlen(search_query) + strlen(e.text.text) < SEARCH_BUFFER_SIZE - 1) {
                    strcat(search_query, e.text.text);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        render_search_bar(renderer, font);
        if (currentLayout) {
            render_content(renderer, font, currentLayout);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    if (currentLayout) {
        free_layout(currentLayout);
    }
}
