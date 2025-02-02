#include "layout.h"
#include "css.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// for my block rendering
static const int blockIndent = 20;      // Indentation for block children.
static const int verticalMargin = 10;   // Vertical margin between lines.
static const int inlineMargin = 5;      // Horizontal margin between inline boxes.
static const int defaultWindowWidth = 800;
static const int baseMargin = 20;       // Left margin from window edge.

// --- Helper Functions to Classify Elements ---

// Return 1 if tag is considered block-level.
static int is_block_element(const char *tag) {
    if (!tag) return 0;
    if (strcasecmp(tag, "div") == 0 ||
        strcasecmp(tag, "p") == 0 ||
        strcasecmp(tag, "h1") == 0 ||
        strcasecmp(tag, "h2") == 0 ||
        strcasecmp(tag, "h3") == 0)
        return 1;
    return 0;
}

// Return 1 if tag is considered inline.
static int is_inline_element(const char *tag) {
    if (!tag) return 0;
    // Treat common inline elements.
    if (strcasecmp(tag, "span") == 0 ||
        strcasecmp(tag, "b") == 0 ||
        strcasecmp(tag, "i") == 0 ||
        strcasecmp(tag, "small") == 0 ||
        strcasecmp(tag, "u") == 0 ||
        strcasecmp(tag, "em") == 0 ||
        strcasecmp(tag, "strong") == 0)
        return 1;
    // Also treat text nodes as inline.
    if (strcmp(tag, "#text") == 0) return 1;
    return 0;
}

// --- Helper: Parse Dimension from Computed Style ---
// Converts a string such as "600px" to an integer.
static int parse_dimension(const char* dim_str, int default_value) {
    if (!dim_str) return default_value;
    int value = atoi(dim_str); // This ignores non-digit characters.
    return (value > 0) ? value : default_value;
}

// --- The Core Layout Function ---
//
// Parameters:
//   node: the current DOM node to layout.
//   layout: pointer to the Layout structure (which holds an array of LayoutBox).
//   base_x: starting x coordinate for the current container.
//   current_y: pointer to the current vertical position (this value is updated as boxes are placed).
//   current_inline_x: pointer to the current x position for inline elements on the same line.
//   available_width: horizontal space available from base_x to the right edge.
//
static void layout_node(DOMNode* node, Layout* layout, int base_x, int *current_y, int *current_inline_x, int available_width) {
    if (!node || !node->name)
        return;

    // Skip <script> and <style> nodes entirely.
    if (strcasecmp(node->name, "script") == 0 || strcasecmp(node->name, "style") == 0)
        return;

    // If the node is block-level, flush any inline content and create a new block.
    if (is_block_element(node->name)) {
        // Flush inline: reset current inline x.
        *current_inline_x = base_x;

        // Determine defaults for this block.
        int default_height = 30;
        if (strcasecmp(node->name, "h1") == 0) default_height = 40;
        else if (strcasecmp(node->name, "h2") == 0) default_height = 35;
        else if (strcasecmp(node->name, "h3") == 0) default_height = 30;
        // For paragraphs and divs, keep the default.

        int width = available_width;  // in other for Block takes full available width.
        int height = default_height;
        if (node->style && node->style->width)
            width = parse_dimension(node->style->width, available_width);
        if (node->style && node->style->height)
            height = parse_dimension(node->style->height, default_height);

        // Create the block layout box.
        LayoutBox box;
        box.x = base_x;
        box.y = *current_y;
        box.width = width;
        box.height = height;
        box.node = node;
        layout->boxes = realloc(layout->boxes, sizeof(LayoutBox) * (layout->count + 1));
        layout->boxes[layout->count++] = box;

        // Update vertical position for next block.
        *current_y += height + verticalMargin;
        // For children of a block, indent them.
        int child_inline_x = base_x + blockIndent;
        int child_avail_width = available_width - blockIndent;
        for (int i = 0; i < node->children_count; i++) {
            layout_node(node->children[i], layout, base_x + blockIndent, current_y, &child_inline_x, child_avail_width);
        }
        // Reset inline x after processing block children.
        *current_inline_x = base_x;
    }
    // Else if the node is inline-level, add it to the current inline container.
    else if (is_inline_element(node->name)) {
        int inline_width = 0;
        int inline_height = 20; // default inline height.
        if (strcmp(node->name, "#text") == 0) {
            // Skip if the text is only whitespace.
            int hasVisible = 0;
            if (node->text) {
                for (const char *p = node->text; *p; p++) {
                    if (!isspace((unsigned char)*p)) { hasVisible = 1; break; }
                }
            }
            if (!hasVisible)
                return;
            // Approximate width: 7 pixels per character.
            inline_width = strlen(node->text) * 7;
        } else {
            // For other inline elements, use a default width.
            inline_width = 50;
            if (node->style && node->style->width)
                inline_width = parse_dimension(node->style->width, inline_width);
        }
        if (node->style && node->style->height)
            inline_height = parse_dimension(node->style->height, inline_height);

        // Check if this inline element fits in the remaining space.
        if ((*current_inline_x) + inline_width > base_x + available_width) {
            // Move to next line.
            *current_y += inline_height + verticalMargin;
            *current_inline_x = base_x;
        }

        // Create the inline layout box.
        LayoutBox box;
        box.x = *current_inline_x;
        box.y = *current_y;
        box.width = inline_width;
        box.height = inline_height;
        box.node = node;
        layout->boxes = realloc(layout->boxes, sizeof(LayoutBox) * (layout->count + 1));
        layout->boxes[layout->count++] = box;

        // Update current inline x.
        *current_inline_x += inline_width + inlineMargin;

        // Process any children inline.
        for (int i = 0; i < node->children_count; i++) {
            layout_node(node->children[i], layout, *current_inline_x, current_y, current_inline_x, base_x + available_width - (*current_inline_x - base_x));
        }
    }
    // Otherwise, process children recursively without adding a box.
    else {
        for (int i = 0; i < node->children_count; i++) {
            layout_node(node->children[i], layout, base_x, current_y, current_inline_x, available_width);
        }
    }
}

Layout* layout_dom(DOMNode* root) {
    Layout* layout = malloc(sizeof(Layout));
    if (!layout) return NULL;
    layout->boxes = NULL;
    layout->count = 0;

    int current_y = 10;            // Top margin.
    int base_x = baseMargin;       // Left margin.
    int current_inline_x = base_x;
    int avail_width = defaultWindowWidth - 2 * baseMargin;

    layout_node(root, layout, base_x, &current_y, &current_inline_x, avail_width);

    return layout;
}

void free_layout(Layout* layout) {
    if (!layout)
        return;
    free(layout->boxes);
    free(layout);
}
