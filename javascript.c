#include "javascript.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./duktape.h"  // Make sure duktape.c/d.h are in your include path

/*
  A very naive approach:
  - Create one Duktape heap.
  - Recursively find all <script> nodes in the DOM.
  - Evaluate their text content on that heap.
  (No DOM integration, no browser APIs – just raw JavaScript evaluation.)
*/

static void traverse_and_run_scripts(DOMNode* node, duk_context* ctx) {
    if (!node)
        return;

    // Check if this is a <script> element.
    if (node->name && strcasecmp(node->name, "script") == 0) {
        if (node->text) {
            if (duk_peval_string(ctx, node->text) != 0) {
                fprintf(stderr, "Script error: %s\n", duk_safe_to_string(ctx, -1));
            }
            // Remove the eval result (or error) from the stack.
            duk_pop(ctx);
        }
    }

    // Recurse into children.
    for (int i = 0; i < node->children_count; i++) {
        traverse_and_run_scripts(node->children[i], ctx);
    }
}

void run_scripts_in_dom(DOMNode* root) {
    duk_context* ctx = duk_create_heap_default();
    if (!ctx) {
        fprintf(stderr, "Failed to create a Duktape heap.\n");
        return;
    }
    traverse_and_run_scripts(root, ctx);
    duk_destroy_heap(ctx);
}
