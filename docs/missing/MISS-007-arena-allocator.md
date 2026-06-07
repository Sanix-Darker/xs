# MISS-007 — Arena allocator for DOM/strings

- **Priority:** P1 (core memory architecture; the biggest lever)
- **Files:** new `arena.{c,h}`, `parser.c`, `strpool.c`, `document.c`

## Problem

Every `DOMNode`, every string (`name`, `text`, `href`), every attribute, and
every CSS string is an individual `malloc`, freed by a deep recursive
`free_dom`. Consequences:

- **High allocator pressure & fragmentation** — thousands of tiny mallocs per
  page; slow construction and teardown.
- **Use-after-free/double-free surface** — manual ownership juggling (FIX-009).
- **Per-allocation overhead** — each malloc carries header + alignment slack;
  with tiny nodes this can dominate (often 16–32 bytes overhead per object).

An arena fixes all three at once and is the foundation for the memory targets in
`00-VISION.md`.

## Design

```c
typedef struct ArenaBlock { struct ArenaBlock* next; size_t used, cap; char data[]; } ArenaBlock;
typedef struct { ArenaBlock* head; size_t default_block; size_t total; } Arena;

Arena* arena_create(size_t default_block);
void*  arena_alloc(Arena*, size_t bytes);          // 8/16-byte aligned
void*  arena_calloc(Arena*, size_t n, size_t sz);
char*  arena_strdup(Arena*, const char* s);
char*  arena_strndup(Arena*, const char* s, size_t n);
void   arena_destroy(Arena*);                      // frees all blocks O(blocks)
size_t arena_bytes(const Arena*);                  // for memory accounting
```

- Bump-pointer within a block; allocate a new block (geometric, or exactly the
  request if larger than `default_block`) when full.
- **No per-object free** — the whole arena is freed at once when the Document is
  destroyed. This makes teardown O(number of blocks), not O(number of nodes),
  and eliminates dangling-pointer bugs within a page.

## Integration

- `Document` owns one `Arena`. `parse_html`, attribute capture, string interning,
  and computed styles all allocate from it.
- `free_dom` is replaced by `arena_destroy` (the recursive free disappears, also
  resolving the FIX-015 deep-recursion-on-free concern).
- Layout boxes stay in their own growable array (rebuilt on relayout), not the
  DOM arena.

## Micro-plan

1. Implement `arena.c` + thorough unit tests (alignment, large allocs, growth,
   byte accounting, destroy).
2. Route `create_dom_node`/string copies/attributes through the arena.
3. Replace `free_dom` calls with `arena_destroy` via `document_free`.
4. Add a memory-accounting hook for the perf harness.

## Acceptance tests

- Unit: alignment of returned pointers; allocations never overlap; total bytes
  match expectations; oversized alloc gets its own block.
- Unit: `arena_strndup` copies exactly n bytes and NUL-terminates.
- Leak: build a Document, destroy it → LeakSan reports 0 leaks; ASan reports no
  UAF when the (now arena-owned) DOM is accessed only before destroy.
- Perf: constructing a 10k-node tree does far fewer `malloc` calls than the
  per-node baseline (counted via a malloc hook in the test).

## Risk

Medium. Central change, but additive and mechanical. Land it early; it
simplifies FIX-009 and FIX-015 and underpins the memory targets.
