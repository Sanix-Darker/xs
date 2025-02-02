#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "parser.h"
#include "javascript.h"
#include "layout.h"
#include "render.h"

// Our browser name
#define BROWSER_NAME "xs"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <url>\n", BROWSER_NAME);
        return EXIT_FAILURE;
    }

    const char* url = argv[1];
    printf("Fetching URL: %s\n", url);

    // 1. Fetch HTML
    char* html = fetch_url(url);
    if (!html) {
        fprintf(stderr, "Failed to fetch HTML from %s\n", url);
        return EXIT_FAILURE;
    }

    // 2. Parse HTML -> build DOM
    DOMNode* dom = parse_html(html);
    if (!dom) {
        fprintf(stderr, "Failed to parse HTML\n");
        free(html);
        return EXIT_FAILURE;
    }

    // 3. Execute any <script> tags (extremely simplified)
    run_scripts_in_dom(dom);

    // 4. Layout (compute minimal geometry)
    Layout* layout = layout_dom(dom);

    // 5. Render (SDL2)
    render_layout(layout);

    // Cleanup
    free_layout(layout);
    free_dom(dom);
    free(html);

    return EXIT_SUCCESS;
}
