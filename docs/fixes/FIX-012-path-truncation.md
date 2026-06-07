# FIX-012 — Font path buffer truncation

- **Severity:** P3 (edge-case robustness)
- **File:** `render.c` (`load_font_path`)

## Problem

```c
static char cwd_font[256];
snprintf(cwd_font, sizeof(cwd_font), "%s", filename);
```

and the Linux path buffers are `[256]`, while `exe_font` is `[1024]`. A long
font filename or deep system path could silently truncate, causing a confusing
"font not found" with no diagnostic. Also `paths[6]` is sized for 6 but only 5
are filled; not a bug, but brittle if more are added.

## Evidence

`render.c load_font_path` buffer declarations.

## Micro-plan

1. Use a single sized buffer per candidate built with `snprintf` and **check the
   return value** against the buffer size; on truncation, skip that candidate
   and log at debug level.
2. Replace the fixed `paths[6]` with a small helper that tries each candidate
   immediately (open-or-continue), avoiding a sized array entirely.
3. Make the DejaVu/Liberation search list a single static table to add paths
   safely.

## Acceptance tests

- Unit (path builder): building a candidate from an over-long filename returns
  "truncated/skip" rather than a half path.
- Manual: fonts still load from exe dir, cwd, and system dirs.

## Risk

Trivial.
