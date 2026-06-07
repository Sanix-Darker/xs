#include "javascript.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mujs/mujs.h"

/*
 * Minimal, read-mostly JS host (MISS-003):
 *   - console.log/info/warn/error  -> stderr (+ test sink)
 *   - navigator.userAgent
 *   - document.title
 *   - document.getElementById(id) -> { textContent, getAttribute(name) }
 *
 * Scripts run once at load with intact source (FIX-002). DOM mutation that
 * would require relayout is intentionally NOT supported yet.
 */

/* The DOM root for the currently-executing script set (single-threaded). */
static DOMNode *g_js_root = NULL;

/* Optional console sink for tests. */
static JsConsoleSink g_console_sink = NULL;
static void *g_console_sink_ctx = NULL;

void js_set_console_sink(JsConsoleSink sink, void *ctx) {
    g_console_sink = sink;
    g_console_sink_ctx = ctx;
}

/* --- helpers --- */

static DOMNode *find_by_id(DOMNode *node, const char *id) {
    if (!node) return NULL;
    if (node->name && node->name[0] != '#') {
        const char *nid = dom_id(node);
        if (nid && strcmp(nid, id) == 0) return node;
    }
    for (int i = 0; i < node->children_count; i++) {
        DOMNode *r = find_by_id(node->children[i], id);
        if (r) return r;
    }
    return NULL;
}

/* Concatenate text content of a node subtree into a malloc'd string. */
static void collect_text(DOMNode *node, char **buf, size_t *len, size_t *cap) {
    if (!node) return;
    if (node->name && strcmp(node->name, "#text") == 0 && node->text) {
        size_t n = strlen(node->text);
        if (*len + n + 1 > *cap) {
            size_t nc = *cap ? *cap : 64;
            while (nc < *len + n + 1) nc <<= 1;
            char *t = realloc(*buf, nc);
            if (!t) return;
            *buf = t; *cap = nc;
        }
        memcpy(*buf + *len, node->text, n);
        *len += n;
        (*buf)[*len] = '\0';
    }
    for (int i = 0; i < node->children_count; i++)
        collect_text(node->children[i], buf, len, cap);
}

/* --- console --- */

static void console_emit(js_State *J, const char *level) {
    int top = js_gettop(J);
    char line[2048];
    size_t off = 0;
    for (int i = 1; i < top; i++) {
        const char *s = js_tostring(J, i);
        int n = snprintf(line + off, sizeof(line) - off, "%s%s",
                         i > 1 ? " " : "", s ? s : "");
        if (n < 0) break;
        off += (size_t)n;
        if (off >= sizeof(line)) { off = sizeof(line) - 1; break; }
    }
    line[off] = '\0';
    if (g_console_sink)
        g_console_sink(g_console_sink_ctx, level, line);
    else
        fprintf(stderr, "[console.%s] %s\n", level, line);
    js_pushundefined(J);
}

static void js_console_log(js_State *J)   { console_emit(J, "log"); }
static void js_console_info(js_State *J)  { console_emit(J, "info"); }
static void js_console_warn(js_State *J)  { console_emit(J, "warn"); }
static void js_console_error(js_State *J) { console_emit(J, "error"); }

/* --- element wrapper --- */

#define ELEM_TAG "xs.Element"

static void elem_getAttribute(js_State *J) {
    DOMNode *node = js_touserdata(J, 0, ELEM_TAG);
    const char *name = js_tostring(J, 1);
    if (node && name) {
        const char *v = dom_attr(node, name);
        if (v) { js_pushstring(J, v); return; }
    }
    js_pushnull(J);
}

static void elem_get_textContent(js_State *J) {
    DOMNode *node = js_touserdata(J, 0, ELEM_TAG);
    char *buf = NULL; size_t len = 0, cap = 0;
    if (node) collect_text(node, &buf, &len, &cap);
    js_pushstring(J, buf ? buf : "");
    free(buf);
}

/* document.getElementById */
static void js_doc_getElementById(js_State *J) {
    const char *id = js_tostring(J, 1);
    DOMNode *node = (id && g_js_root) ? find_by_id(g_js_root, id) : NULL;
    if (!node) { js_pushnull(J); return; }

    /* Build the prototype object (with methods) FIRST; js_newuserdata pops it
       off the stack to use as the new userdata object's prototype. */
    js_newobject(J);
    js_newcfunction(J, elem_getAttribute, "getAttribute", 1);
    js_setproperty(J, -2, "getAttribute");
    js_newcfunction(J, elem_get_textContent, "_getTextContent", 0);
    js_setproperty(J, -2, "_getTextContent");

    js_newuserdata(J, ELEM_TAG, node, NULL);   /* pops prototype, pushes userdata */

    /* expose textContent as a snapshot string property on the instance */
    {
        char *buf = NULL; size_t len = 0, cap = 0;
        collect_text(node, &buf, &len, &cap);
        js_pushstring(J, buf ? buf : "");
        js_setproperty(J, -2, "textContent");
        free(buf);
    }
}

static void register_bindings(js_State *J, DOMNode *root) {
    g_js_root = root;

    /* console */
    js_newobject(J);
    js_newcfunction(J, js_console_log, "log", 1);   js_setproperty(J, -2, "log");
    js_newcfunction(J, js_console_info, "info", 1); js_setproperty(J, -2, "info");
    js_newcfunction(J, js_console_warn, "warn", 1); js_setproperty(J, -2, "warn");
    js_newcfunction(J, js_console_error, "error", 1); js_setproperty(J, -2, "error");
    js_setglobal(J, "console");

    /* navigator */
    js_newobject(J);
    js_pushstring(J, "Mozilla/5.0 (compatible; xs/0.2)");
    js_setproperty(J, -2, "userAgent");
    js_setglobal(J, "navigator");

    /* document */
    js_newobject(J);
    {
        char *title = NULL; size_t len = 0, cap = 0;
        /* find <title> text */
        DOMNode *t = NULL;
        DOMNode *stack[256]; int sp = 0; if (root) stack[sp++] = root;
        while (sp) {
            DOMNode *n = stack[--sp];
            if (n->name && strcmp(n->name, "title") == 0) { t = n; break; }
            for (int i = 0; i < n->children_count && sp < 256; i++) stack[sp++] = n->children[i];
        }
        if (t) collect_text(t, &title, &len, &cap);
        js_pushstring(J, title ? title : "");
        js_setproperty(J, -2, "title");
        free(title);
    }
    js_newcfunction(J, js_doc_getElementById, "getElementById", 1);
    js_setproperty(J, -2, "getElementById");
    js_setglobal(J, "document");

    /* window as an alias-ish global object */
    js_pushglobal(J);
    js_setglobal(J, "window");
}

/* --- script collection + execution --- */

static char *collect_script_text(DOMNode *node) {
    if (!node) return NULL;
    size_t total = 0;
    for (int i = 0; i < node->children_count; i++) {
        DOMNode *child = node->children[i];
        if (child && child->name && strcmp(child->name, "#text") == 0 && child->text)
            total += strlen(child->text);
    }
    if (total == 0) return NULL;
    char *source = malloc(total + 1);
    if (!source) return NULL;
    size_t off = 0;
    for (int i = 0; i < node->children_count; i++) {
        DOMNode *child = node->children[i];
        if (child && child->name && strcmp(child->name, "#text") == 0 && child->text) {
            size_t n = strlen(child->text);
            memcpy(source + off, child->text, n);
            off += n;
        }
    }
    source[off] = '\0';
    return source;
}

static void traverse_and_run_scripts(DOMNode *node, js_State *J) {
    if (!node) return;
    if (node->name && strcmp(node->name, "script") == 0) {
        /* skip external scripts (src=) — not fetched (documented) */
        if (!dom_attr(node, "src")) {
            char *source = collect_script_text(node);
            if (source) {
                if (js_ploadstring(J, "[script]", source) == 0) {
                    js_pushglobal(J);
                    if (js_pcall(J, 0)) {
                        const char *err = js_tostring(J, -1);
                        fprintf(stderr, "Script error: %s\n", err ? err : "?");
                    }
                    js_pop(J, 1);
                } else {
                    const char *err = js_tostring(J, -1);
                    fprintf(stderr, "Script parse error: %s\n", err ? err : "?");
                    js_pop(J, 1);
                }
                free(source);
            }
        }
    }
    for (int i = 0; i < node->children_count; i++)
        traverse_and_run_scripts(node->children[i], J);
}

void run_scripts_in_dom(DOMNode *root) {
    js_State *J = js_newstate(NULL, NULL, 0);
    if (!J) {
        fprintf(stderr, "Failed to create a MuJS state.\n");
        return;
    }
    register_bindings(J, root);
    traverse_and_run_scripts(root, J);
    g_js_root = NULL;
    js_freestate(J);
}
