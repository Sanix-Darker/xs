#include "forms.h"
#include <string.h>

static int is_unreserved(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

size_t form_url_encode(const char* in, char* out, size_t out_sz) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    if (!in || !out || out_sz == 0) return 0;
    for (const unsigned char* p = (const unsigned char*)in; *p; p++) {
        if (is_unreserved(*p)) {
            if (o + 1 >= out_sz) break;
            out[o++] = (char)*p;
        } else if (*p == ' ') {
            if (o + 1 >= out_sz) break;
            out[o++] = '+';
        } else {
            if (o + 3 >= out_sz) break;
            out[o++] = '%';
            out[o++] = hex[(*p >> 4) & 0xF];
            out[o++] = hex[*p & 0xF];
        }
    }
    out[o] = '\0';
    return o;
}

size_t form_build_query(const char* const* names, const char* const* values,
                        int n, char* out, size_t out_sz) {
    size_t o = 0;
    if (!out || out_sz == 0) return 0;
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (!names[i]) continue;
        if (o && o + 1 < out_sz) out[o++] = '&';
        o += form_url_encode(names[i], out + o, out_sz - o);
        if (o + 1 < out_sz) out[o++] = '=';
        o += form_url_encode(values[i] ? values[i] : "", out + o, out_sz - o);
        out[o] = '\0';
    }
    return o;
}

DOMNode* form_owner(DOMNode* node) {
    for (DOMNode* n = node; n; n = n->parent)
        if (n->name && strcmp(n->name, "form") == 0) return n;
    return NULL;
}

static void collect(DOMNode* node, const char** names, const char** values,
                    int max, int* count) {
    if (!node || *count >= max) return;
    if (node->name && (strcmp(node->name, "input") == 0 ||
                       strcmp(node->name, "textarea") == 0 ||
                       strcmp(node->name, "select") == 0)) {
        const char* nm = dom_attr(node, "name");
        if (nm) {
            const char* type = dom_attr(node, "type");
            /* skip submit/button/reset controls as fields */
            if (!type || (strcmp(type, "submit") != 0 && strcmp(type, "button") != 0 &&
                          strcmp(type, "reset") != 0)) {
                const char* val = dom_attr(node, "value");
                names[*count] = nm;
                values[*count] = val ? val : "";
                (*count)++;
            }
        }
    }
    for (int i = 0; i < node->children_count; i++)
        collect(node->children[i], names, values, max, count);
}

int form_collect_fields(DOMNode* form,
                        const char** names, const char** values, int max) {
    int count = 0;
    collect(form, names, values, max, &count);
    return count;
}
