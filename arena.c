#include "arena.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ARENA_DEFAULT_BLOCK (64u * 1024u)   /* 64 KB blocks */
#define ARENA_ALIGN (sizeof(void *) > 8 ? sizeof(void *) : 16)

static size_t align_up(size_t n, size_t align) {
    size_t rem = n % align;
    return rem ? n + (align - rem) : n;
}

Arena *arena_create(size_t default_block) {
    Arena *a = xs_malloc(sizeof *a);
    if (!a) return NULL;
    a->head = NULL;
    a->default_block = default_block ? default_block : ARENA_DEFAULT_BLOCK;
    a->total_allocated = 0;
    a->block_count = 0;
    return a;
}

static ArenaBlock *arena_new_block(Arena *a, size_t min_payload) {
    size_t cap = a->default_block;
    if (cap < min_payload) cap = min_payload;
    /* overflow guard on header + cap */
    if (cap > (size_t)-1 - sizeof(ArenaBlock)) return NULL;
    ArenaBlock *b = xs_malloc(sizeof(ArenaBlock) + cap);
    if (!b) return NULL;
    b->next = a->head;
    b->used = 0;
    b->cap = cap;
    a->head = b;
    a->block_count++;
    return b;
}

void *arena_alloc(Arena *a, size_t bytes) {
    if (!a) return NULL;
    if (bytes == 0) bytes = 1;
    size_t need = align_up(bytes, ARENA_ALIGN);
    if (need < bytes) return NULL; /* alignment overflow */

    ArenaBlock *b = a->head;
    if (!b || b->used + need > b->cap) {
        /* Need a fresh block. Large requests get their own exact-ish block. */
        b = arena_new_block(a, need);
        if (!b) return NULL;
    }
    void *p = b->data + b->used;
    b->used += need;
    a->total_allocated += bytes;
    return p;
}

void *arena_calloc(Arena *a, size_t count, size_t size) {
    if (size != 0 && count > (size_t)-1 / size) return NULL; /* overflow */
    size_t total = count * size;
    void *p = arena_alloc(a, total);
    if (p) memset(p, 0, total);
    return p;
}

char *arena_strdup(Arena *a, const char *s) {
    if (!s) return NULL;
    return arena_strndup(a, s, strlen(s));
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;
    char *out = arena_alloc(a, len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

void *arena_realloc(Arena *a, void *old, size_t old_size, size_t new_size) {
    if (new_size == 0) return NULL;
    void *p = arena_alloc(a, new_size);
    if (!p) return NULL;
    if (old && old_size) {
        size_t copy = old_size < new_size ? old_size : new_size;
        memcpy(p, old, copy);
    }
    return p;
}

size_t arena_bytes(const Arena *a) {
    return a ? a->total_allocated : 0;
}

void arena_destroy(Arena *a) {
    if (!a) return;
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    free(a);
}
