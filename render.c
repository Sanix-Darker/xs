#include "render.h"
#include "layout.h"
#include "parser.h"
#include "css.h"  // Include CSS header to access computed style definitions.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Global scroll offset. Positive values scroll down.
static int scroll_offset = 0;

// Helper: Parse a hex color string (e.g. "#RRGGBB") into RGB values.
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

// Helper: Render text using SDL_ttf.
static void render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {
    SDL_Color textColor = {0, 0, 0, 255};  // black text
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, textColor);
    if (!textSurface) {
        fprintf(stderr, "Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(textSurface);
        return;
    }
    SDL_Rect renderQuad = { x, y, textSurface->w, textSurface->h };
    SDL_FreeSurface(textSurface);
    SDL_RenderCopy(renderer, textTexture, NULL, &renderQuad);
    SDL_DestroyTexture(textTexture);
}

void render_layout(Layout* layout) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        SDL_Quit();
        return;
    }

    // Create a window.
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

    // Use SDL_GetBasePath() to load the font from the executable's directory.
    char *base_path = SDL_GetBasePath();
    char font_path[1024];
    strncpy(font_path, "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", sizeof(font_path));
    TTF_Font *font = TTF_OpenFont(font_path, 16);
    if (!font) {
        fprintf(stderr, "Failed to load font from '%s'! SDL_ttf Error: %s\n", font_path, TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return;
    }

    bool running = true;
    SDL_Event e;
    while (running) {
        // Process events.
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            else if (e.type == SDL_MOUSEWHEEL) {
                // Mouse wheel: scroll_offset adjusted by wheel amount.
                scroll_offset += e.wheel.y * 20;
            }
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    // hjkl vim navigation, because why not ?
                    case SDLK_UP:
                    case SDLK_k:
                        scroll_offset += 20;
                        break;
                    case SDLK_DOWN:
                    case SDLK_j:
                        scroll_offset -= 20;
                        break;
                    default:
                        break;
                }
            }
        }

        // Clear the screen with a white background.
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        // Render each layout box.
        for (int i = 0; i < layout->count; i++) {
            LayoutBox *box = &layout->boxes[i];

            // Filter: Only render if the node is a <div> or a text node.
            if (box->node && box->node->name) {
                if ((strcasecmp(box->node->name, "div") != 0) &&
                    (strcasecmp(box->node->name, "#text") != 0)) {
                    continue; // Skip nodes other than <div> and text.
                }
            }

            // Adjust Y coordinate by scroll offset.
            SDL_Rect rect = { box->x, box->y + scroll_offset, box->width, box->height };

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

            // If it's a text node, render its text with horizontal alignment.
            if (box->node && strcmp(box->node->name, "#text") == 0 && box->node->text) {
                int text_w, text_h;
                TTF_SizeUTF8(font, box->node->text, &text_w, &text_h);
                int text_x = rect.x + 5; // default left padding.
                if (box->node->style && box->node->style->text_align) {
                    if (strcasecmp(box->node->style->text_align, "center") == 0) {
                        text_x = rect.x + (rect.w - text_w) / 2;
                    } else if (strcasecmp(box->node->style->text_align, "right") == 0) {
                        text_x = rect.x + rect.w - text_w - 5;
                    }
                    // Otherwise, default to left alignment.
                }
                render_text(renderer, font, box->node->text, text_x, rect.y + 5);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);  // Roughly 60 FPS.
    }

    // Cleanup.
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}
