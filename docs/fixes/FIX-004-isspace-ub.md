# FIX-004 — `isspace(char)` undefined behavior on signed chars

- **Severity:** P1 (UB; wrong behavior on non-ASCII)
- **Files:** `css.c` (`trim`), any `ctype.h` use with a plain `char`

## Problem

`css.c trim()`:

```c
while (isspace(*str)) str++;          // *str is char (may be negative)
...
while (len > 0 && isspace(str[len-1])) len--;
```

`isspace`/`isalpha`/etc. require their argument to be representable as
`unsigned char` or `EOF`. On platforms where `char` is signed (x86/ARM default),
a byte ≥ 0x80 (any UTF-8 continuation/lead byte, common in real CSS/text) is
passed as a negative `int`, which is undefined behavior and in practice can
misclassify bytes or index a ctype table out of bounds.

`layout.c` `has_visible_text` already casts correctly (`(const unsigned char*)`)
— good — but `css.c` does not, and any new ctype use must follow the rule.

## Evidence

`css.c` `trim` lines using `isspace(*str)` / `isspace(str[len-1])`.

## Micro-plan

1. Audit all `ctype.h` calls: `css.c`, `layout.c`, `render.c`, new files.
2. Wrap every argument: `isspace((unsigned char)c)`.
3. Add a tiny helper `static inline int xs_isspace(int c)` in a shared header to
   make intent explicit and avoid repeating the cast.

## Acceptance tests

- Unit test: `trim("\xC3\xA9 abc \xC3\xA9")` (UTF-8 "é") returns
  `"\xC3\xA9 abc \xC3\xA9"` unchanged at the ends (non-space bytes are not
  trimmed) and does not crash under UBSan.
- Build under `-fsanitize=undefined` and run CSS tests with no UBSan reports.

## Risk

Trivial. Pure correctness hardening.
