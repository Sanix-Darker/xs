#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "network.h"
#include "parser.h"
#include "document.h"
#include "layout.h"
#include "render.h"

#define BROWSER_NAME "xs"
#define XS_VERSION   "0.2.0"

typedef struct {
    const char *url;
    int   dump;        /* 0 none, 1 layout dump, 2 tree dump */
    int   width;
    int   height;
    int   profile;     /* print phase timings + counts */
} Options;

static void print_usage(FILE *out) {
    fprintf(out,
        "Usage: %s [options] <url|file|path>\n"
        "\n"
        "Options:\n"
        "  --dump          Lay out the page and print boxes, then exit.\n"
        "  --dump=tree     Print the DOM tree instead of layout boxes.\n"
        "  --width=N       Viewport width for layout/dump (default 950).\n"
        "  --height=N      Viewport height (default 700).\n"
        "  --profile       Print fetch/parse/layout timings and counts.\n"
        "  --version       Print version and exit.\n"
        "  --help          Print this help and exit.\n",
        BROWSER_NAME);
}

/* Print a DOM tree (--dump=tree). */
static void dump_tree(DOMNode *node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) fputs("  ", stdout);
    if (node->name && strcmp(node->name, "#text") == 0) {
        printf("#text \"%s\"\n", node->text ? node->text : "");
    } else {
        printf("%s", node->name ? node->name : "?");
        if (node->href) printf(" href=%s", node->href);
        putchar('\n');
    }
    for (int i = 0; i < node->children_count; i++)
        dump_tree(node->children[i], depth + 1);
}

static int parse_args(int argc, char **argv, Options *opt) {
    opt->url = NULL;
    opt->dump = 0;
    opt->width = 950;
    opt->height = 700;
    opt->profile = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0) { print_usage(stdout); exit(EXIT_SUCCESS); }
        else if (strcmp(a, "--version") == 0) {
            printf("%s %s\n", BROWSER_NAME, XS_VERSION);
            exit(EXIT_SUCCESS);
        }
        else if (strcmp(a, "--dump") == 0) opt->dump = 1;
        else if (strcmp(a, "--dump=tree") == 0) opt->dump = 2;
        else if (strcmp(a, "--profile") == 0) opt->profile = 1;
        else if (strncmp(a, "--width=", 8) == 0) {
            int v = atoi(a + 8); if (v > 0) opt->width = v;
        }
        else if (strncmp(a, "--height=", 9) == 0) {
            int v = atoi(a + 9); if (v > 0) opt->height = v;
        }
        else if (strncmp(a, "--", 2) == 0) {
            fprintf(stderr, "Unknown option: %s\n", a);
            return 0;
        }
        else {
            opt->url = a;  /* first non-flag is the URL */
        }
    }
    return 1;
}

/* Count DOM nodes (for --profile). */
static int count_nodes(DOMNode *n) {
    if (!n) return 0;
    int c = 1;
    for (int i = 0; i < n->children_count; i++) c += count_nodes(n->children[i]);
    return c;
}

static double ms_since(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_nsec - a.tv_nsec) / 1e6;
}

/* Headless path: fetch -> build document -> (layout dump | tree dump). */
static int run_dump(const Options *opt) {
    struct timespec t0, t1, t2, t3;
    FetchResult fr;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (!fetch_into(opt->url, &fr)) {
        fprintf(stderr, "Failed to fetch: %s\n", opt->url);
        return EXIT_FAILURE;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    Document *doc = document_from_bytes(fr.body, fr.length, fr.final_url, fr.charset);
    size_t body_len = fr.length;
    fetch_result_free(&fr);
    if (!doc) {
        fprintf(stderr, "Failed to build document\n");
        return EXIT_FAILURE;
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    if (opt->profile) {
        /* layout once for timing (separate from any dump) */
        Layout *lay = layout_dom(doc->root, NULL, opt->width);
        clock_gettime(CLOCK_MONOTONIC, &t3);
        printf("PROFILE fetch=%.2fms build=%.2fms layout=%.2fms total=%.2fms "
               "body=%zuB nodes=%d boxes=%zu\n",
               ms_since(t0, t1), ms_since(t1, t2), ms_since(t2, t3),
               ms_since(t0, t3), body_len, count_nodes(doc->root),
               lay ? lay->count : 0);
        if (lay) { lay->dom = NULL; free_layout(lay); }
        document_free(doc);
        return EXIT_SUCCESS;
    }

    if (opt->dump == 2) {
        dump_tree(doc->root, 0);
    } else {
        /* font == NULL -> deterministic headless measurement model. */
        Layout *lay = layout_dom(doc->root, NULL, opt->width);
        dump_layout(lay, stdout, opt->width, opt->height);
        if (lay) { lay->dom = NULL; free_layout(lay); }  /* doc owns the DOM */
    }

    document_free(doc);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    Options opt;
    if (!parse_args(argc, argv, &opt)) { print_usage(stderr); return EXIT_FAILURE; }
    if (!opt.url) {
        fprintf(stderr, "Error: no URL provided.\n\n");
        print_usage(stderr);
        return EXIT_FAILURE;
    }

    network_init();
    document_set_fetcher(fetch_url);   /* enable external <link> CSS (FEAT-014) */

    int rc;
    if (opt.dump || opt.profile) {
        rc = run_dump(&opt);
        network_cleanup();
        return rc;
    }

    /* Interactive path. */
    printf("Fetching URL: %s\n", opt.url);
    FetchResult fr;
    if (!fetch_into(opt.url, &fr) || !fr.body) {
        fprintf(stderr, "Failed to fetch HTML from %s\n", opt.url);
        fetch_result_free(&fr);
        network_cleanup();
        return EXIT_FAILURE;
    }

    Document *doc = document_from_bytes(fr.body, fr.length, fr.final_url, fr.charset);
    fetch_result_free(&fr);
    if (!doc) {
        fprintf(stderr, "Failed to parse HTML\n");
        network_cleanup();
        return EXIT_FAILURE;
    }

    /* render_layout takes ownership of the DOM tree. Detach it from the
       Document so document_free does not double-free. */
    DOMNode *dom = doc->root;
    doc->root = NULL;
    document_free(doc);

    render_layout(dom, opt.url);

    network_cleanup();
    return EXIT_SUCCESS;
}
