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
        // out of memory
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, total_size);
    mem->size += total_size;
    mem->data[mem->size] = '\0';

    return total_size;
}

char* fetch_url(const char* url) {
    CURL* curl_handle;
    CURLcode res;

    struct Memory chunk;
    chunk.data = malloc(1);  // will grow as needed
    if (!chunk.data) {
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = curl_easy_init();
    if (!curl_handle) {
        curl_global_cleanup();
        free(chunk.data);
        return NULL;
    }

    // Set the URL to fetch.
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    // Set the callback function to write the fetched data.
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&chunk);
    // Set a custom User-Agent.
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "xs/0.1");

    // Enable automatic redirection for HTTP 301, 302, etc.
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    // Optionally, set a maximum number of redirections (default is usually 50).
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);

    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.data);
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        return NULL;
    }

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    return chunk.data; // caller must free the allocated memory.
}
