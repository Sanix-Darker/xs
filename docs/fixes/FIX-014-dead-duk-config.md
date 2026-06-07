# FIX-014 — Unused `duk_config.h` dead file

- **Severity:** P3 (repo hygiene)
- **File:** `duk_config.h` (repo root)

## Problem

`duk_config.h` (3804 lines) is a Duktape configuration header. The project uses
**MuJS** for JavaScript, not Duktape, and nothing in the build or source
`#include`s `duk_config.h`. It is dead weight that confuses readers about which
JS engine is in use.

## Evidence

- `grep -r duk_config .` → only the file itself.
- Build sources list MuJS (`mujs/one.c`), no Duktape.

## Micro-plan

1. Confirm zero references (`grep -rn "duk_config\|duktape\|duk_" --include=*.c
   --include=*.h .` excluding the file itself).
2. Remove `duk_config.h`.
3. Add a short note in `docs/02-ARCHITECTURE.md` (already states MuJS) recording
   the engine decision so the question does not recur.

## Acceptance tests

- Build still succeeds after removal (it must, since it was unreferenced).
- `grep` shows no remaining references.

## Risk

None, provided the grep confirms no references.
