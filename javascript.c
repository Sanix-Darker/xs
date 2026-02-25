#include "javascript.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mujs/mujs.h"

/*
  A very naive approach:
  - Create one MuJS state.
  - Recursively find all <script> nodes in the DOM.
  - Evaluate the concatenated #text children as JavaScript source.
  (No DOM integration, no browser APIs; just raw JavaScript evaluation.)
*/

static char *collect_script_text(DOMNode *node) {
    if (!node) return NULL;

    size_t total = 0;
    for (int i = 0; i < node->children_count; i++) {
        DOMNode *child = node->children[i];
        if (!child || !child->name || strcmp(child->name, "#text") != 0 || !child->text) {
            continue;
        }
        total += strlen(child->text);
    }

    if (total == 0) return NULL;

    char *source = malloc(total + 1);
    if (!source) return NULL;

    size_t off = 0;
    for (int i = 0; i < node->children_count; i++) {
        DOMNode *child = node->children[i];
        if (!child || !child->name || strcmp(child->name, "#text") != 0 || !child->text) {
            continue;
        }
        size_t n = strlen(child->text);
        memcpy(source + off, child->text, n);
        off += n;
    }
    source[off] = '\0';
    return source;
}

static void traverse_and_run_scripts(DOMNode* node, js_State* J) {
    if (!node)
        return;

    if (node->name && strcasecmp(node->name, "script") == 0) {
        char *source = collect_script_text(node);
        if (source) {
            if (js_dostring(J, source)) {
                fprintf(stderr, "Script error in <script> block\n");
            }
            free(source);
        }
    }

    for (int i = 0; i < node->children_count; i++) {
        traverse_and_run_scripts(node->children[i], J);
    }
}

void run_scripts_in_dom(DOMNode* root) {
    js_State* J = js_newstate(NULL, NULL, 0);
    if (!J) {
        fprintf(stderr, "Failed to create a MuJS state.\n");
        return;
    }
    traverse_and_run_scripts(root, J);
    js_freestate(J);
}
