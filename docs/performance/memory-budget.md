# Memory Budget

Per-subsystem byte budgets for a **typical article** (~1500 words, ~600
elements). These are targets that the test-suite memory counters guard against
regressing. Numbers are order-of-magnitude design budgets, refined as real
measurements land in `benchmarks.md`.

## Per-node footprint

Current `DOMNode` (pointers + ints + separate string mallocs):

- struct: ~ 8 (name) + 8 (text) + 8 (href) + 8 (children ptr) + 4+4 (count/cap)
  + 8 (style ptr) ≈ 48 bytes, **plus** malloc header (~16) = ~64 bytes.
- strings: `name` (own malloc + header), `text` (own malloc + header) — easily
  another 40–80 bytes per node.
- Per-word shredding multiplies node count ~5–10× for text.

Target `DOMNode` (arena, interned, typed):

- struct: name-ptr(interned, shared) 8 + text-ptr 8 + len 4 + tag(enum) 2 +
  flags 2 + parent 8 + children 8 + count/cap 8 + attrs 8 + style 8 ≈ 64 bytes,
  **no per-node malloc header** (arena), strings interned/shared.
- No per-word nodes (FEAT-011): text stays as whole-node spans.

### Budget

| Subsystem | Current (est.) | Target | Lever |
|-----------|---------------:|-------:|-------|
| DOM nodes (600 elems) | ~600×~120B + word nodes | < 256 KB | arena + no shredding |
| Word/text nodes | ~1500×~100B ≈ 150 KB | 0 (none) | FEAT-011 |
| Strings (names/text) | many small mallocs | interned, ~ text size once | strpool + arena |
| Computed styles | 5 strings/node when set | typed, ~48B/node when set | FEAT-005 |
| Layout boxes | ~count × ~56B | ~count × ~40B | bitfield hints |
| Texture cache | 1/word, unbounded | bounded LRU (e.g. 8 MB) | FIX-011 |
| CSS stylesheet | small | small | — |
| **Total (typical)** | **often 40–80 MB** | **< 20 MB RSS** | all of the above |

## Hard caps (safety, FIX-015)

| Cap | Default | Rationale |
|-----|--------:|-----------|
| `XS_MAX_DOCUMENT_BYTES` | 32 MB | reject runaway responses |
| `XS_MAX_NODES` | 2,000,000 | bound DOM size |
| `XS_MAX_DEPTH` | 512 | bound recursion/stack |
| Texture cache budget | 8 MB | bound glyph memory |
| Image decode budget | 64 MB (when images on) | bound pixel memory |
| Visited set | 4096 entries | bound history memory |

## Accounting hooks

- `arena_bytes(arena)` — live DOM/string bytes, asserted per fixture in tests.
- test malloc counter — total allocations per fixture, asserted under a budget.
- texture cache exposes `count`/`bytes` for the bounded-growth test.

## Regression guards

A unit test parses `tests/fixtures/article.html` (a ~1500-word fixture) and
asserts:

- arena bytes < budget,
- DOM node count == O(elements) (not O(words)),
- malloc count < budget.

If a change blows a budget, the test fails and the number must be justified and
the budget updated deliberately.
