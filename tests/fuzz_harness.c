/*
 * Fuzz / crash-corpus harness (testing/fuzzing.md).
 *
 * libFuzzer is not available in the macOS Command Line Tools, so this is the
 * AFL-style fallback: it reads one input file per argument (or stdin), runs the
 * full document+layout pipeline on each, and exits 0 if none crash. Build it
 * under ASan/UBSan and run it over tests/fixtures + tests/fixtures/crash to
 * catch regressions:
 *
 *   cmake -S . -B build-asan -DXS_SANITIZE=ON
 *   cmake --build build-asan --target xs_fuzz
 *   ./build-asan/xs_fuzz  tests/fixtures/basic.html
 */
#include "../document.h"
#include "../layout.h"
#include "../util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void run_one(const char *data, size_t len) {
    if (len > XS_MAX_DOCUMENT_BYTES) return;
    Document *d = document_from_bytes(data, len, "file:///fuzz", NULL);
    if (d) {
        Layout *l = layout_dom(d->root, NULL, 800);
        if (l) { l->dom = NULL; free_layout(l); }
        document_free(d);
    }
}

static char *read_all(FILE *f, size_t *out_len) {
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t n;
    while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            char *t = realloc(buf, cap);
            if (!t) { free(buf); return NULL; }
            buf = t;
        }
    }
    *out_len = len;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        size_t len = 0;
        char *buf = read_all(stdin, &len);
        if (buf) { run_one(buf, len); free(buf); }
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) { fprintf(stderr, "skip %s\n", argv[i]); continue; }
        size_t len = 0;
        char *buf = read_all(f, &len);
        fclose(f);
        if (buf) {
            run_one(buf, len);
            free(buf);
            fprintf(stderr, "ok %s (%zu bytes)\n", argv[i], len);
        }
    }
    return 0;
}
