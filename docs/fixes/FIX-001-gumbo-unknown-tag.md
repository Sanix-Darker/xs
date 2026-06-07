# FIX-001 — Heap over-read on unknown tag names

- **Severity:** P0 (memory corruption / heap over-read)
- **File:** `parser.c`

## Problem

In `parse_gumbo_node`:

```c
const char* tag_name = gumbo_normalized_tagname(element->tag);
if (strcmp(tag_name, "UNKNOWN") == 0) {
    tag_name = element->original_tag.data;   // <-- NOT nul-terminated
}
DOMNode* node = create_dom_node(tag_name, NULL);  // strdup(tag_name)
```

`element->original_tag` is a `GumboStringPiece`:

```c
typedef struct { const char* data; size_t length; } GumboStringPiece;
```

`gumbo.h` explicitly says string pieces "are assumed ... not NUL-terminated"
and point into the original input buffer. Calling `strdup` on `.data` reads
from the tag start until it happens to hit a `\0` somewhere later in the HTML
buffer — copying the rest of the document as the "tag name". This is a heap
over-read and produces garbage node names (which then drive layout/CSS).

Real-world trigger: any custom element or unknown/SVG/MathML tag, e.g.
`<my-widget>`, `<svg>`, `<math>`, `<x-foo>` — extremely common on modern sites.

## Evidence

`gumbo_src/gumbo.h` lines ~82–91 (string piece contract) and the `original_tag`
field doc at ~486–491.

## Micro-plan

1. Replace the unsafe assignment with a length-bounded copy. Add a helper:

   ```c
   static DOMNode* create_dom_node_n(const char* name, size_t name_len,
                                     const char* text);
   ```

   that copies exactly `name_len` bytes and NUL-terminates.
2. For the unknown-tag branch, also run `gumbo_tag_from_original_text` semantics
   is already applied by Gumbo for `original_tag` (it points at just the tag
   name region), so using `original_tag.data` + `original_tag.length` is
   correct. Use the length.
3. If `original_tag.length == 0` (algorithmically inserted), fall back to a
   stable string like `"unknown"`.
4. Lowercase the copied tag name (Gumbo normalized names are upper for the enum
   path but original text preserves case; our classification is
   case-insensitive, but interning later wants canonical lowercase).

## Acceptance tests

- Unit test: parse `"<svg><circle></circle></svg><x-foo>hi</x-foo>"` and assert
  node names are exactly `svg`, `circle`, `x-foo` (lowercased), with no trailing
  garbage and correct length.
- ASan: parsing a buffer ending immediately after `<x-` (truncated) does not
  read past the buffer.

## Risk

Low. Bounded copy is strictly safer. Must ensure the fallback for zero-length
pieces.
