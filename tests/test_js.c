#include "test_util.h"
#include "../parser.h"
#include "../javascript.h"
#include <string.h>
#include <stdio.h>

static char g_capture[4096];
static void sink(void *ctx, const char *level, const char *msg) {
    (void)ctx;
    size_t off = strlen(g_capture);
    snprintf(g_capture + off, sizeof(g_capture) - off, "%s:%s\n", level, msg);
}

TEST(js_console_log) {
    g_capture[0] = '\0';
    js_set_console_sink(sink, NULL);
    DOMNode *root = parse_html("<script>console.log('hello', 1 + 2);</script>");
    run_scripts_in_dom(root);
    js_set_console_sink(NULL, NULL);
    CHECK(strstr(g_capture, "log:hello 3") != NULL);
    free_dom(root);
}

TEST(js_intact_source_runs) {
    /* FIX-002 regression: whitespace-significant source must execute. */
    g_capture[0] = '\0';
    js_set_console_sink(sink, NULL);
    DOMNode *root = parse_html(
        "<script>var a = 1 + 2; if (a > 2) { console.log('ok'); }</script>");
    run_scripts_in_dom(root);
    js_set_console_sink(NULL, NULL);
    CHECK(strstr(g_capture, "log:ok") != NULL);
    free_dom(root);
}

TEST(js_document_title) {
    g_capture[0] = '\0';
    js_set_console_sink(sink, NULL);
    DOMNode *root = parse_html(
        "<title>My Page</title><script>console.log(document.title);</script>");
    run_scripts_in_dom(root);
    js_set_console_sink(NULL, NULL);
    CHECK(strstr(g_capture, "log:My Page") != NULL);
    free_dom(root);
}

TEST(js_getElementById) {
    g_capture[0] = '\0';
    js_set_console_sink(sink, NULL);
    DOMNode *root = parse_html(
        "<div id=\"x\" data-v=\"42\">Hello</div>"
        "<script>var e=document.getElementById('x');"
        "console.log(e.getAttribute('data-v'), e.textContent);</script>");
    run_scripts_in_dom(root);
    js_set_console_sink(NULL, NULL);
    CHECK(strstr(g_capture, "log:42 Hello") != NULL);
    free_dom(root);
}

TEST(js_getElementById_missing) {
    g_capture[0] = '\0';
    js_set_console_sink(sink, NULL);
    DOMNode *root = parse_html(
        "<script>console.log(document.getElementById('nope'));</script>");
    run_scripts_in_dom(root);
    js_set_console_sink(NULL, NULL);
    CHECK(strstr(g_capture, "log:null") != NULL);
    free_dom(root);
}

TEST(js_syntax_error_no_crash) {
    /* A broken script must not crash the host. */
    DOMNode *root = parse_html("<script>this is ) not valid (((</script>");
    run_scripts_in_dom(root);   /* should print an error and continue */
    CHECK(1);
    free_dom(root);
}
