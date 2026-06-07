#include "document.h"
#include "css.h"
#include "javascript.h"
#include "encoding.h"
#include "ua_css.h"
#include "url.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static ResourceFetcher g_fetcher = NULL;

void document_set_fetcher(ResourceFetcher fetcher) { g_fetcher = fetcher; }

/* Find the first <title> element and concatenate its text children. */
static DOMNode *find_first(DOMNode *node, const char *name) {
    if (!node) return NULL;
    if (node->name && strcmp(node->name, name) == 0) return node;
    for (int i = 0; i < node->children_count; i++) {
        DOMNode *r = find_first(node->children[i], name);
        if (r) return r;
    }
    return NULL;
}

char *document_extract_title(DOMNode *root) {
    DOMNode *title = find_first(root, "title");
    if (!title) return NULL;

    /* Concatenate direct #text children. */
    size_t total = 0;
    for (int i = 0; i < title->children_count; i++) {
        DOMNode *c = title->children[i];
        if (c->name && strcmp(c->name, "#text") == 0 && c->text)
            total += strlen(c->text);
    }
    if (total == 0) return NULL;

    char *out = xs_malloc(total + 1);
    if (!out) return NULL;
    size_t off = 0;
    for (int i = 0; i < title->children_count; i++) {
        DOMNode *c = title->children[i];
        if (c->name && strcmp(c->name, "#text") == 0 && c->text) {
            size_t n = strlen(c->text);
            memcpy(out + off, c->text, n);
            off += n;
        }
    }
    out[off] = '\0';
    return out;
}

/* Collect author stylesheet *sources* in document order: <style> text and the
   fetched contents of external <link rel=stylesheet> (FEAT-014). Each source
   is applied to the document root in order so author cascade order is correct. */
static void collect_and_apply_css(DOMNode* node, DOMNode* root,
                                  const char* base_url) {
    if (!node || !node->name) return;

    if (strcmp(node->name, "style") == 0) {
        size_t total = 0;
        for (int i = 0; i < node->children_count; i++)
            if (node->children[i]->text) total += strlen(node->children[i]->text) + 1;
        if (total) {
            char* buf = xs_malloc(total + 1);
            if (buf) {
                size_t off = 0;
                for (int i = 0; i < node->children_count; i++) {
                    const char* t = node->children[i]->text;
                    if (!t) continue;
                    size_t n = strlen(t);
                    memcpy(buf + off, t, n); off += n; buf[off++] = '\n';
                }
                buf[off] = '\0';
                CSSStyleSheet* s = parse_css(buf);
                if (s) { apply_stylesheet_to_dom(s, root); free_stylesheet(s); }
                free(buf);
            }
        }
        return;
    }

    if (strcmp(node->name, "link") == 0 && g_fetcher) {
        const char* rel = dom_attr(node, "rel");
        const char* href = dom_attr(node, "href");
        if (rel && href && strstr(rel, "stylesheet")) {
            char abs[2048];
            url_resolve(base_url, href, abs, sizeof abs);
            char* css = g_fetcher(abs);
            if (css) {
                CSSStyleSheet* s = parse_css(css);
                if (s) { apply_stylesheet_to_dom(s, root); free_stylesheet(s); }
                free(css);
            }
        }
        return;
    }

    for (int i = 0; i < node->children_count; i++)
        collect_and_apply_css(node->children[i], root, base_url);
}

Document *document_from_html(const char *html, const char *base_url) {
    (void)base_url;
    if (!html) return NULL;

    /* 1. Parse HTML -> DOM. */
    DOMNode *root = parse_html(html);
    if (!root) return NULL;

    /* 2. Extract <title> BEFORE any text mutation (title is raw-text and is
          skipped by split, but extract here for clarity/order). */
    char *title = document_extract_title(root);

    /* 3. Apply CSS. UA stylesheet first (lowest priority), then author CSS
          (<style> + external <link>) in document order (FIX-002 / FEAT-014/15). */
    {
        CSSStyleSheet *ua = parse_css(XS_UA_CSS);
        if (ua) { apply_stylesheet_to_dom(ua, root); free_stylesheet(ua); }
    }
    collect_and_apply_css(root, root, base_url);

    /* Inheritance pass once, after UA + author cascade (FEAT-005). */
    style_inherit(root);

    /* 4. Run <script> tags with intact source (FIX-002). */
    run_scripts_in_dom(root);

    /* 5. (FEAT-011) Word breaking is done at layout time, not by mutating the
          DOM. The DOM keeps whole text nodes — script/style/pre text stays
          intact and node count stays O(structure). */

    Document *doc = xs_calloc(1, sizeof *doc);
    if (!doc) {
        free(title);
        free_dom(root);
        return NULL;
    }
    doc->root = root;
    doc->title = title;
    return doc;
}

void document_free(Document *doc) {
    if (!doc) return;
    if (doc->title) free(doc->title);
    if (doc->root) free_dom(doc->root);
    free(doc);
}

Document *document_from_bytes(const char *bytes, size_t len,
                              const char *base_url, const char *charset) {
    if (!bytes) return NULL;
    Encoding enc = encoding_detect(charset, bytes, len);
    size_t utf8_len = 0;
    char *utf8 = encoding_to_utf8(enc, bytes, len, &utf8_len);
    if (!utf8) return NULL;
    Document *doc = document_from_html(utf8, base_url);
    free(utf8);
    return doc;
}
