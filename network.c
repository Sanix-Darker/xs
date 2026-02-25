#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct Memory {
    char* data;
    size_t size;
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    struct Memory* mem = (struct Memory*)userp;

    char* ptr = realloc(mem->data, mem->size + total_size + 1);
    if (ptr == NULL) {
        return 0;
    }

    mem->data = ptr;
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

char* fetch_url(const char* url) {
    CURL* curl_handle;
    CURLcode res;

    struct Memory chunk;
    chunk.data = malloc(1);
    if (!chunk.data) {
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }
    chunk.size = 0;

    curl_handle = curl_easy_init();
    if (!curl_handle) {
        free(chunk.data);
        return NULL;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl_handle, CURLOPT_ENCODING, "");       /* auto gzip/deflate */
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L);       /* 30s timeout       */

    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.data);
        curl_easy_cleanup(curl_handle);
        return NULL;
    }

    curl_easy_cleanup(curl_handle);
    return chunk.data;
}
