# FEAT-011 — Move word breaking into layout

- **Priority:** P1 (fixes FIX-002/FIX-003 structurally; big memory win)
- **Files:** `parser.c` (remove split), new `text.{c,h}`, `layout.c`

## Why

`split_text_nodes` mutates the DOM into one node per word. Consequences:

- Destroys `<script>`/`<style>` text (FIX-002).
- Mishandles tabs/newlines (FIX-003).
- Explodes node count (a 1000-word article → 1000+ nodes) → high memory,
  high allocator pressure, huge texture-cache churn (one texture per word).
- Couples a layout concern (wrapping) to the DOM.

The correct design keeps whole text nodes in the DOM and breaks into words/runs
**at layout time**, where the available width and font are known.

## Design

`text.c` provides a whitespace-aware word iterator over a text node:

```c
typedef struct { const char* start; int len; int trailing_space; } TextRun;
typedef struct { const char* p; const char* end; int pre; } TextIter;
void text_iter_init(TextIter*, const char* s, int pre);
int  text_iter_next(TextIter*, TextRun* out);   // 0 when done
```

- Non-pre: collapse runs of html-space to a single break opportunity; each
  `TextRun` is a maximal non-space token; `trailing_space=1` if a collapsible
  space follows.
- Pre: preserve bytes; break on `\n` into lines, keep tabs/spaces.

Layout, when it reaches a `#text` node, iterates runs and:

- measures each run at the true font size (FIX-008),
- places it at the inline cursor, wrapping when it would overflow,
- emits a `LayoutBox` per **run** (not per word in the DOM) — boxes are
  layout-owned and transient, so they do not bloat the DOM and are rebuilt on
  relayout.

To keep the texture cache effective and bounded, boxes reference the original
text node plus a `(offset, len)` substring, and the cache is keyed by content
hash of that substring + style (FIX-011).

## Micro-plan

1. Implement `text.c` iterator + unit tests (whitespace, pre, unicode-safe byte
   handling — never split inside a UTF-8 sequence; we only split on ASCII
   space/tab/newline which are never UTF-8 continuation bytes, so byte-splitting
   is safe).
2. Add a substring view to `LayoutBox` (`const char* text; int text_len;`) or
   keep emitting one box per run with a pointer+len into the node text.
3. Replace the per-word layout path to use the iterator; remove the
   `split_text_nodes` call from the pipeline.
4. Update render to draw `text[0..text_len)` (use `TTF_RenderUTF8` on a
   temporary NUL-terminated copy or a length-aware render helper).
5. Delete `split_text_nodes` (or keep dormant + untested behind a flag during
   transition, then remove).

## Acceptance tests

- Unit: iterator over `"  the  quick\nbrown "` → runs `the`,`quick`,`brown` with
  correct trailing-space flags; pre mode preserves bytes.
- Unit: layout of a known paragraph at a known width produces the same line
  breaks as before (golden), but the DOM node count is unchanged by layout
  (no new DOM nodes created).
- Memory: node count for a 1000-word fixture is O(structure), not O(words).

## Risk

Medium–high. This is the central layout refactor. Land it with goldens captured
from the pre-refactor build so wrapping behavior is preserved or intentionally
improved.
