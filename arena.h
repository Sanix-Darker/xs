#ifndef XS_ARENA_H
#define XS_ARENA_H

#include <stddef.h>

/*
 * Bump-pointer arena allocator (MISS-007).
 *
 * All allocations for a Document (DOM nodes, strings, attributes, computed
 * styles) come from one Arena. Teardown frees all blocks at once in
 * O(#blocks), eliminating per-node free walks and whole classes of
 * use-after-free / double-free bugs.
 *
 * There is intentionally no per-allocation free: free the whole arena.
 */

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t used;
    size_t cap;
    /* data[] follows the header (flexible array member) */
    char data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;        /* most-recently-allocated block */
    size_t default_block;    /* default new-block size */
    size_t total_allocated;  /* sum of bytes handed out (for accounting) */
    size_t block_count;
} Arena;

/* Create an arena. default_block is the size of standard blocks (bytes).
   Pass 0 for a sensible default. Returns NULL on allocation failure. */
Arena *arena_create(size_t default_block);

/* Allocate `bytes` aligned to the platform max alignment. NULL on failure. */
void *arena_alloc(Arena *a, size_t bytes);

/* Allocate zero-initialized memory for count*size bytes. NULL on failure. */
void *arena_calloc(Arena *a, size_t count, size_t size);

/* Duplicate a NUL-terminated string into the arena. NULL on failure. */
char *arena_strdup(Arena *a, const char *s);

/* Duplicate at most n bytes (stopping early at a NUL) and NUL-terminate. */
char *arena_strndup(Arena *a, const char *s, size_t n);

/* "Reallocate" within the arena: allocate `new_size` bytes, copy `old_size`
   bytes from `old` (may be NULL). The old block is abandoned (freed with the
   arena). Geometric growth keeps total waste bounded. NULL on failure. */
void *arena_realloc(Arena *a, void *old, size_t old_size, size_t new_size);

/* Total bytes handed out to callers (excludes block headers/slack). */
size_t arena_bytes(const Arena *a);

/* Free every block and the arena itself. Safe on NULL. */
void arena_destroy(Arena *a);

#endif /* XS_ARENA_H */
