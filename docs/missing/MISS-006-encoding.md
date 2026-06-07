# MISS-006 — Character encoding detection

- **Priority:** P1 (mojibake on non-UTF-8 pages)
- **Files:** `network.c`, `document.c`, new `encoding.{c,h}`

## Problem

Input is assumed UTF-8. SDL_ttf renders UTF-8. Pages served as ISO-8859-1
(Latin-1), Windows-1252, or other legacy encodings (still common on older sites)
render as mojibake or dropped glyphs. There is no charset detection from the
HTTP header or `<meta charset>`.

## Scope (pragmatic)

Support the encodings that cover the long tail of the readable web:

1. **UTF-8** (pass through, validate).
2. **ISO-8859-1 / Windows-1252** → transcode to UTF-8 (a 1-byte→UTF-8 table;
   1252 maps 0x80–0x9F to specific code points).
3. Detection order (per HTML spec, simplified):
   - HTTP `Content-Type; charset=` (FIX-010).
   - `<meta charset=...>` / `<meta http-equiv content="...charset=...">` in the
     first N KB.
   - BOM sniff (UTF-8/UTF-16 BOM).
   - Default: UTF-8, with a Latin-1 fallback if UTF-8 validation fails badly.
4. UTF-16 → transcode to UTF-8 (less common; P3).

## Micro-plan

1. `encoding.c`: `utf8_validate`, `latin1_to_utf8`, `cp1252_to_utf8`,
   `detect_charset(header_charset, head_bytes)`.
2. In `document_from_fetch`, transcode the body to UTF-8 **once** before
   parsing, based on the detected charset.
3. Re-check `<meta charset>` (a cheap scan of the first 1 KB) since the header
   may be absent.

## Acceptance tests

- Unit: `cp1252_to_utf8("\x91hi\x92")` → curly-quote UTF-8 sequences.
- Unit: `latin1_to_utf8("\xE9")` → "é" (`\xC3\xA9`).
- Unit: `detect_charset` prefers header, then meta, then BOM, then default.
- Unit: invalid UTF-8 falls back to Latin-1 transcode (no crash, legible-ish).

## Risk

Low–medium. Transcoding tables are mechanical and fully unit testable. Keep the
set small (UTF-8, Latin-1, 1252) per the minimal mission.
