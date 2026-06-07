# FIX-011 — Texture cache keyed by raw text pointer

- **Severity:** P1 (stale/incorrect glyphs across navigation)
- **File:** `render.c`

## Problem

The texture cache keys entries on the **address** of `node->text`:

```c
TCacheEntry { const char *key; int font_size; int bold; SDL_Texture *tex; ... }
tcache_lookup(text /* pointer */, fs, bold);
```

Pointer identity is not stable across the document lifetime:

- After navigation the old DOM is freed; the allocator can hand the **same
  address** to a different string in the new page → a lookup "hits" and renders
  the wrong text.
- This is currently masked by `tcache_clear()` on every navigate/resize, which
  also throws away **all** cached glyphs — so the cache provides no cross-page
  benefit and is one missed `clear()` away from a correctness bug.

## Evidence

`render.c` `tcache_hash`/`tcache_lookup`/`tcache_insert` use `const char* key`
== the node text pointer; `tcache_clear` is called in `navigate_to`, resize, and
the history handlers.

## Micro-plan

1. With string interning (PERF-005 / strpool), key the cache on the **interned
   string id** (a stable integer for the page) plus size/bold/italic. Interned
   strings live in the document arena, so identity is stable for the page and
   distinct strings never collide.
2. Better still for cross-page reuse: key on a 64-bit **content hash** of the
   text + style. Then identical words across pages share a texture and the cache
   need not be cleared on navigation (only evicted by capacity/LRU).
3. Add an **eviction policy** (capacity cap + LRU) so the cache cannot grow
   without bound on long sessions (ties to PERF / FIX-015).
4. Tie texture lifetime to the cache, not the DOM; clearing the DOM must not
   require clearing textures if keyed by content hash.

## Acceptance tests

- Unit (hash keying, headless logic test without SDL textures): inserting two
  distinct strings that would alias under pointer-keying produces two distinct
  cache entries; identical content+style resolves to one entry.
- Stress: simulate 100 navigations; cache entry count stays bounded by the cap;
  no assertion of wrong-text (content hash match implies equal bytes — verify
  by storing and comparing the bytes on collision).

## Risk

Medium. Content-hash collisions must fall back to byte comparison to guarantee
correctness. Bounded cache needs an eviction list.
