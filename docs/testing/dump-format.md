# `--dump` Output Contract

`--dump` is the backbone of golden testing. Its format is **stable**; changing
it requires bumping the version line and regenerating goldens deliberately.

## Invocation

```
xs --dump [--dump=tree] [--width=N] [--height=N] <file://path | url | path>
```

- No SDL window is created. Layout uses the headless (font==NULL) measurement
  model so coordinates are machine-independent.
- Exit code 0 on success, non-zero on load/parse failure.

## Layout dump format (`--dump`, default)

First line is a version header:

```
XS-DUMP 1 width=<W> height=<H>
```

Then one line per `LayoutBox`, in layout (paint) order:

```
BOX x=<int> y=<int> w=<int> h=<int> fs=<int> flags=<F> tag=<name> text="<escaped>"
```

- `flags` is a fixed-order string of single chars, each present or `-`:
  `B`(bold) `I`(italic) `H`(heading) `L`(link) `M`(list-marker) `R`(hr)
  `P`(pre) `Q`(blockquote) `S`(structural-border) `G`(image).
  Example: `flags=B--L------` = bold + link.
- `text` is present only for `#text` boxes; escaped (`\"`, `\\`, `\n`→`\n`).
  Non-text boxes omit the `text=` field.
- `tag` is the (lowercased, interned) tag name of the box's node.

## DOM dump format (`--dump=tree`)

```
XS-TREE 1
<indent>tag [#id] [.class...] [href=...] "text-if-text-node"
```

Indent is two spaces per depth level. Useful for parser/attribute tests
independent of layout.

## Stability rules

1. The version integer increments on any field add/remove/reorder.
2. Goldens live in `tests/golden/` and are regenerated only via a documented
   `make golden` step, then code-reviewed as part of the change.
3. Floating values are rounded to integers (pixel grid) for determinism.
4. The headless measurement model must be deterministic and documented in
   `text.c` (e.g. width = sum of per-char nominal advances scaled by font size).

## Example

```
XS-DUMP 1 width=800 height=700
BOX x=30 y=10 w=740 h=39 fs=28 flags=-H------- tag=h1
BOX x=30 y=10 w=120 h=39 fs=28 flags=BH------- tag=#text text="Hello"
BOX x=30 y=60 w=740 h=22 fs=16 flags=--------- tag=p
BOX x=30 y=60 w=84 h=22 fs=16 flags=--------- tag=#text text="world"
```

(Illustrative; real values come from the headless model.)
