#include "network.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <curl/curl.h>

/* ------------------------------------------------------------------ */
/* Local file loading (FEAT-021): file:// URLs and bare paths.         */

char* fetch_file(const char* path) {
    if (!path || !*path) return NULL;

    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        return NULL;
    }

    /* Determine size (cap-bounded). */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    if ((size_t)sz + 1 > XS_MAX_DOCUMENT_BYTES) {
        fprintf(stderr, "File exceeds size cap: %s\n", path);
        fclose(f);
        return NULL;
    }

    char* buf = xs_malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* Map a file:// URL or bare path to a filesystem path (into dst). Returns 1 if
   the input is a local path, 0 if it should be treated as a network URL. */
static int local_path_of(const char* url, char* dst, size_t dst_sz) {
    if (!url) return 0;
    if (strncmp(url, "file://", 7) == 0) {
        const char* p = url + 7;
        /* file://localhost/... or file:///... -> skip optional host */
        if (strncmp(p, "localhost/", 10) == 0) p += 9;     /* leave leading '/' */
        /* file:///abs -> p points at "/abs"; file://rel unusual, take as-is */
        snprintf(dst, dst_sz, "%s", p);
        return 1;
    }
    /* Bare path: no scheme "://" present means local file. */
    if (!strstr(url, "://")) {
        snprintf(dst, dst_sz, "%s", url);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */

struct Memory {
    char*  data;
    size_t size;      /* bytes used */
    size_t cap;       /* bytes allocated */
    int    overflow;  /* set if the size cap was exceeded */
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    struct Memory* mem = (struct Memory*)userp;

    /* FIX-015: reject runaway responses. Abort the transfer by returning a
       short count, which makes libcurl fail with CURLE_WRITE_ERROR. */
    if (mem->size + total_size + 1 > XS_MAX_DOCUMENT_BYTES) {
        mem->overflow = 1;
        return 0;
    }

    /* Geometric growth to avoid O(n^2) reallocation on large bodies. */
    if (mem->size + total_size + 1 > mem->cap) {
        size_t newcap = mem->cap ? mem->cap : 16384;
        while (newcap < mem->size + total_size + 1) {
            if (newcap > XS_MAX_DOCUMENT_BYTES) { newcap = mem->size + total_size + 1; break; }
            newcap <<= 1;
        }
        char* ptr = xs_realloc(mem->data, newcap);
        if (!ptr) return 0;
        mem->data = ptr;
        mem->cap = newcap;
    }

    memcpy(&(mem->data[mem->size]), contents, total_size);
    mem->size += total_size;
    mem->data[mem->size] = '\0';

    return total_size;
}

void network_init(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void network_cleanup(void) {
    curl_global_cleanup();
}

/* Parse "text/html; charset=utf-8" into type and charset (lowercased). */
void parse_content_type(const char* header, char* type, size_t type_sz,
                        char* charset, size_t charset_sz) {
    if (type && type_sz) type[0] = '\0';
    if (charset && charset_sz) charset[0] = '\0';
    if (!header) return;

    /* type = up to ';' or end, trimmed + lowercased */
    const char* p = header;
    while (*p && xs_is_html_space((unsigned char)*p)) p++;
    size_t ti = 0;
    while (*p && *p != ';' && !xs_is_html_space((unsigned char)*p)) {
        if (type && ti + 1 < type_sz) type[ti++] = (char)xs_tolower((unsigned char)*p);
        p++;
    }
    if (type && type_sz) type[ti] = '\0';

    /* find charset= */
    const char* c = header;
    while ((c = strchr(c, '=')) != NULL) {
        /* back up to see if preceded by "charset" */
        const char* k = c;
        while (k > header && xs_is_html_space((unsigned char)k[-1])) k--;
        if (k - header >= 7) {
            const char* kw = k - 7;
            if (strncasecmp(kw, "charset", 7) == 0) {
                const char* v = c + 1;
                while (*v && (xs_is_html_space((unsigned char)*v) || *v == '"')) v++;
                size_t ci = 0;
                while (*v && *v != ';' && *v != '"' && !xs_is_html_space((unsigned char)*v)) {
                    if (charset && ci + 1 < charset_sz)
                        charset[ci++] = (char)xs_tolower((unsigned char)*v);
                    v++;
                }
                if (charset && charset_sz) charset[ci] = '\0';
                return;
            }
        }
        c++;
    }
}

/* Guess content-type from a local file extension. */
static void content_type_from_ext(const char* path, char* type, size_t type_sz) {
    const char* dot = strrchr(path, '.');
    const char* t = "application/octet-stream";
    if (dot) {
        if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) t = "text/html";
        else if (strcasecmp(dot, ".css") == 0) t = "text/css";
        else if (strcasecmp(dot, ".txt") == 0) t = "text/plain";
        else if (strcasecmp(dot, ".js") == 0)  t = "application/javascript";
        else if (strcasecmp(dot, ".json") == 0) t = "application/json";
        else if (strcasecmp(dot, ".png") == 0) t = "image/png";
        else if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) t = "image/jpeg";
        else if (strcasecmp(dot, ".gif") == 0) t = "image/gif";
        else t = "text/html"; /* default to html for unknown local docs */
    } else {
        t = "text/html";
    }
    snprintf(type, type_sz, "%s", t);
}

void fetch_result_free(FetchResult* out) {
    if (!out) return;
    if (out->body) free(out->body);
    out->body = NULL;
    out->length = 0;
}

int fetch_into(const char* url, FetchResult* out) {
    if (!out) return 0;
    memset(out, 0, sizeof *out);
    if (!url) return 0;
    snprintf(out->final_url, sizeof out->final_url, "%s", url);

    /* Local files bypass libcurl. */
    char local[4096];
    if (local_path_of(url, local, sizeof local)) {
        char* body = fetch_file(local);
        if (!body) return 0;
        out->body = body;
        out->length = strlen(body);
        out->status = 200;
        content_type_from_ext(local, out->content_type, sizeof out->content_type);
        out->ok = 1;
        return 1;
    }

    struct Memory chunk = { xs_malloc(1), 0, 1, 0 };
    if (!chunk.data) return 0;
    chunk.data[0] = '\0';

    CURL* h = curl_easy_init();
    if (!h) { free(chunk.data); return 0; }

    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(h);
    if (res != CURLE_OK) {
        if (chunk.overflow)
            fprintf(stderr, "Response exceeded size cap (%u bytes): %s\n",
                    (unsigned)XS_MAX_DOCUMENT_BYTES, url);
        else
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.data);
        curl_easy_cleanup(h);
        return 0;
    }

    long status = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    out->status = status;

    char* ct = NULL;
    curl_easy_getinfo(h, CURLINFO_CONTENT_TYPE, &ct);
    if (ct) parse_content_type(ct, out->content_type, sizeof out->content_type,
                               out->charset, sizeof out->charset);

    char* eff = NULL;
    curl_easy_getinfo(h, CURLINFO_EFFECTIVE_URL, &eff);
    if (eff) snprintf(out->final_url, sizeof out->final_url, "%s", eff);

    curl_easy_cleanup(h);

    out->body = chunk.data;
    out->length = chunk.size;
    out->ok = (status > 0 && status < 400);
    return 1;
}

char* fetch_url(const char* url) {
    FetchResult fr;
    if (!fetch_into(url, &fr)) return NULL;
    char* body = fr.body;   /* transfer ownership */
    fr.body = NULL;
    fetch_result_free(&fr);
    return body;
}
