#ifndef RENDER_H
#define RENDER_H

#include "parser.h"

// Creates an SDL window, lays out the DOM, and runs the event loop.
// Takes ownership of the DOM tree (will be freed on exit).
void render_layout(DOMNode *dom, const char *initial_url);

#endif
