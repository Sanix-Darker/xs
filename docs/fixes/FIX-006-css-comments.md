# FIX-006 — CSS comments desync the parser

- **Severity:** P1 (wrong output; can drop or merge rules)
- **File:** `css.c`

## Problem

`parse_css` has no handling for `/* ... */` comments. A stylesheet like:

```css
/* nav { display: none } */
p { color: black; }
```

is parsed as a rule with selector `/* nav` and a declaration whose value spans
into the next block, corrupting subsequent parsing. Comments containing `{`,
`}`, `:`, or `;` (very common) desynchronize the flat scanner. Real stylesheets
almost always contain comments and license banners.

## Evidence

`css.c parse_css` scans for `{`, `}`, `:`, `;` literally with no comment state.

## Micro-plan

1. Rewrite the scanner as a small tokenizer that, at every position, first skips
   comments and whitespace via a `skip_ws_and_comments(const char** p)` helper:
   - whitespace run, then
   - if `p[0]=='/' && p[1]=='*'`, advance to the closing `*/` (or EOF).
   - repeat until neither matches.
2. Call this skip at every structural boundary: before selector, inside the
   declaration block before a property, after `:`, and before `;`/`}`.
3. Also strip comments that appear **inside** a value (e.g.
   `color: red /* x */ ;`).
4. Guard against unterminated comments (run to EOF, stop cleanly).

## Acceptance tests

- Unit test: `"/* a { b */ p { color: red; }"` yields exactly one rule,
  selector `p`, one declaration `color: red`.
- Unit test: `"p{color:red /*x*/;}"` yields value `red` (comment stripped).
- Unit test: unterminated `"p{color:red; /* oops"` does not crash and yields the
  `color` declaration.

## Risk

Low–medium. The tokenizer rewrite is localized to `css.c` and well-covered by
tests.
