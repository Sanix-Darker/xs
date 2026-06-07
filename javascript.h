#ifndef JAVASCRIPT_H
#define JAVASCRIPT_H

#include "parser.h"

/* Console output sink (MISS-003). Receives the level ("log"/"warn"/...) and the
   formatted message. Used by tests to capture console output; when unset,
   console writes to stderr. */
typedef void (*JsConsoleSink)(void *ctx, const char *level, const char *msg);
void js_set_console_sink(JsConsoleSink sink, void *ctx);

void run_scripts_in_dom(DOMNode* root);

#endif
