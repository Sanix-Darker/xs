#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "parser.h"
#include "javascript.h"
#include "layout.h"
#include "render.h"
#include "css.h"

#define BROWSER_NAME "xs"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <url>\n", BROWSER_NAME);
        return EXIT_FAILURE;
    }

    const char* url = argv[1];
    printf("Fetching URL: %s\n", url);

    network_init();

    // 1. Fetch HTML
    char* html = fetch_url(url);
    if (!html) {
        fprintf(stderr, "Failed to fetch HTML from %s\n", url);
        network_cleanup();
        return EXIT_FAILURE;
    }

    // 2. Parse HTML -> build DOM
    DOMNode* dom = parse_html(html);
    free(html);
    if (!dom) {
        fprintf(stderr, "Failed to parse HTML\n");
        network_cleanup();
        return EXIT_FAILURE;
    }

    // 2b. Split text nodes into words for wrapping
    split_text_nodes(dom);

    // 3. Apply CSS from <style> tags
    char* style_text = extract_style_text(dom);
    if (style_text) {
        CSSStyleSheet* sheet = parse_css(style_text);
        if (sheet) {
            apply_stylesheet_to_dom(sheet, dom);
            free_stylesheet(sheet);
        }
        free(style_text);
    }

    // 4. Execute any <script> tags (extremely simplified)
    run_scripts_in_dom(dom);

    // 5. Render (SDL2) — takes ownership of dom
    render_layout(dom, url);

    network_cleanup();
    return EXIT_SUCCESS;
}
