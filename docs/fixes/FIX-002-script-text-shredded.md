# FIX-002 — JavaScript source destroyed by word-splitting

- **Severity:** P1 (feature entirely broken)
- **Files:** `main.c`, `render.c` (`reload_page`)

## Problem

The pipeline order is:

```c
split_text_nodes(dom);     // shreds every space-containing #text into words
...
run_scripts_in_dom(dom);   // concatenates #text children of <script>
```

`split_text_nodes` walks **all** nodes, including `<script>` and `<style>`
children, and splits their text on spaces into many one-word `#text` nodes,
**discarding the spaces**. `collect_script_text` then concatenates those word
nodes with no separators, so:

```js
var x = 1; console.log(x + 2);
```

becomes the single token stream:

```
varx=1;console.log(x+2);
```

which is not valid JS (`varx` is one identifier). Scripts that rely on any
whitespace-significant construct (almost all) fail to parse or behave wrongly.
The same shredding also corrupts `<style>` text, but CSS is extracted in
`main.c` *before* the split in `main.c` — yet in `render.c reload_page` the
split happens *before* CSS extraction too, so navigation-time CSS is also
corrupted. The two code paths are inconsistent.

## Evidence

`parser.c split_text_nodes` recurses into every child unconditionally;
`javascript.c collect_script_text` concatenates with no separator.

## Micro-plan

The clean fix combines with FIX-003 and FEAT-011 (stop per-word shredding
entirely). Concretely:

1. **Do not split text inside `<script>`, `<style>`, `<pre>`, `<textarea>`.**
   `split_text_nodes` must skip raw-text/whitespace-significant elements.
2. **Extract CSS and run scripts from the original (unsplit) text.** Reorder so
   that style extraction and script execution read the DOM **before** any text
   transformation, OR (preferred) eliminate per-word shredding from the DOM and
   move word breaking into layout (FEAT-011). With layout-time wrapping, the DOM
   keeps whole text nodes and scripts/CSS are always intact.
3. Make `main.c` and `render.c reload_page` use the **same** ordering via a
   single shared `build_document(html)` function so the two paths cannot drift.

## Decision

Adopt FEAT-011 (layout-time word breaking). Then `split_text_nodes` is removed
from the pipeline and this bug disappears structurally — `<script>`/`<style>`
text is never mutated. Until FEAT-011 lands, apply step (1) as an interim guard.

## Acceptance tests

- Unit test: build a document from
  `"<script>var a = 1 + 2; if (a > 2) a = a + 1;</script>"`, collect script
  source, assert it equals the original (spaces preserved).
- Unit test: `<style>div { color: red; }</style>` round-trips intact into the
  CSS extractor.
- Integration (`--dump`): a page whose script sets nothing visible still parses
  without a MuJS syntax error on stderr.

## Risk

Medium — touches pipeline ordering. Mitigated by routing both entry points
through one `build_document`.
