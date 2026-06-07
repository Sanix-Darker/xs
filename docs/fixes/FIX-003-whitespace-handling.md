# FIX-003 — Tabs/newlines mishandled in text splitting

- **Severity:** P1 (wrong output / wasted memory)
- **File:** `parser.c`

## Problem

`split_text_nodes` splits only on the ASCII space `' '`:

```c
while (*p && *p != ' ') p++;   // newline/tab are treated as part of a word
```

Real HTML text is full of `\n` and `\t` from source formatting. The HTML
whitespace rule (outside `<pre>`) is: any run of spaces, tabs, newlines, and
form feeds collapses to a single space, and leading/trailing runs at block
edges are trimmed. The current code:

- Treats `"foo\nbar"` as the single word `foo\nbar` (no break, renders with a
  literal newline the font draws as a box or nothing, and never wraps there).
- Keeps tabs inside words.
- Only trims leading spaces, not tabs/newlines.

This produces mis-wrapped lines, stray glyphs, and inconsistent spacing on
nearly every real page.

## Evidence

`parser.c` `split_text_nodes`, the inner `while (*p && *p != ' ')` loops and the
leading-trim `while (*src == ' ')`.

## Micro-plan

Resolved together with FEAT-011 (move breaking to layout). The canonical
solution is a **whitespace-collapsing tokenizer** in `text.c`:

1. Define `is_html_space(c)` = c ∈ {space, \t, \n, \r, \f}.
2. When iterating a text node for layout, collapse any run of html-space to a
   single break opportunity (a single space of width = space advance).
3. Preserve all whitespace verbatim inside `<pre>`/`<textarea>` (significant
   whitespace context flag threaded through layout).
4. Trim leading/trailing collapsible whitespace at block boundaries.

Until FEAT-011, if any interim splitting remains, treat all html-space as the
delimiter and drop empty tokens.

## Acceptance tests

- Unit test (text iterator): `"  foo\n\t bar   baz "` yields tokens
  `["foo","bar","baz"]` with single-space separators and no leading/trailing
  space.
- Unit test (pre context): same input under `pre=true` preserves the exact
  bytes including the newline and tab.

## Risk

Low–medium. Whitespace rules are fiddly; covered by explicit unit tests.
