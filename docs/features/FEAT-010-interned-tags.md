# FEAT-010 — Interned tag enum + parent pointers

- **Priority:** P2 (perf + unblocks selectors)
- **Files:** new `strpool.{c,h}`, `parser.{c,h}`, `dom.{c,h}`, `layout.c`

## Why

Two structural gaps:

1. **No parent pointers** on `DOMNode` → descendant selector matching
   (FEAT-004) and upward style inheritance walks are impossible/awkward.
2. **String-typed tags** → every classification (`IS_BLOCK`, `IS_INLINE`,
   heading check, `icmp("div")...`) does repeated `strcasecmp`. On a large page
   laid out repeatedly (resize), this is a real hot path.

## Design

- `strpool`: intern byte spans into stable, arena-owned strings; return a
  `const char*` plus an integer id. Identical strings share storage and id.
- `TagId`: a precomputed enum for known HTML tags
  (`TAG_DIV`, `TAG_P`, `TAG_A`, `TAG_TEXT`, ..., `TAG_UNKNOWN`). At node
  creation, map the (lowercased) tag name to a `TagId` via a perfect/hash
  lookup once. Store `node->tag` (enum) alongside `node->name` (interned
  string for unknowns/serialization).
- `DOMNode.parent` pointer set during tree construction.

Then:

- Block/inline classification becomes a table lookup by `TagId` (O(1), no
  strcmp).
- Heading detection: `tag >= TAG_H1 && tag <= TAG_H6`.
- The texture cache and CSS class matching benefit from interned strings.

## Micro-plan

1. Implement `strpool` (hash map, arena-backed). Unit test intern identity.
2. Generate a `TagId` enum + a name→id lookup (sorted table + binary search, or
   gperf-style; keep it simple with a sorted table reusing `tag_tables.h`).
3. Add `tag` and `parent` to `DOMNode`; set them in `parse_gumbo_node`.
4. Refactor `layout.c` classification to use `TagId` (keep string path as
   fallback for unknown tags).
5. Keep `node->name` for unknown/custom tags and debugging/dump.

## Acceptance tests

- Unit: `strpool_intern("div")` twice returns the same pointer and id.
- Unit: `tag_id("DIV")==tag_id("div")==TAG_DIV`; unknown → `TAG_UNKNOWN`.
- Unit: after parse, every node's `parent` points to its real parent; root's is
  NULL.
- Unit: block/inline classification via `TagId` matches the old string-based
  result for all tags in the tables.

## Risk

Medium. Touches the node struct and parse. Additive fields (keep `name`) keep
the blast radius small; classification refactor is mechanical and test-locked.
