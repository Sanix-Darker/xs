# FIX-007 — CSS lengths ignore units and percentages

- **Severity:** P1 (wrong layout)
- **Files:** `layout.c` (`parse_dimension`), `css.c`/`style.c`

## Problem

`layout.c parse_dimension` uses `strtol` and ignores the unit suffix:

```c
long v = strtol(s, &end, 10);   // "50%" -> 50, "2em" -> 2, "10pt" -> 10
```

So `width: 50%` becomes 50 **pixels**, `font-size: 1.2em` becomes 1px (the
leading `1`), `width: 100%` becomes 100px. Any non-`px` unit is silently wrong,
and fractional values are truncated to their integer prefix.

## Evidence

`layout.c parse_dimension` and its callers (`css_font_size`, block width).

## Micro-plan

Introduce typed lengths in `style.c`:

1. `typedef enum { LEN_AUTO, LEN_PX, LEN_PERCENT, LEN_EM } LengthUnit;`
   `typedef struct { float value; LengthUnit unit; } Length;`
2. `Length parse_length(const char* s)` that recognizes `px`, `%`, `em`, `rem`,
   `pt`, and unitless (treated as px for legacy / 0). Unknown → `LEN_AUTO`.
3. Resolve at layout time against the containing block:
   - `LEN_PX` → value.
   - `LEN_PERCENT` → `value/100 * containing_width` (for widths) or font size
     (for `font-size`).
   - `LEN_EM` → `value * current_font_size`; `pt` → `value * 96/72`.
4. Store `font-size` as a resolved pixel size in `ComputedStyle` after cascade.

## Acceptance tests

- Unit: `parse_length("50%")` → `{50, LEN_PERCENT}`; `"1.5em"` →
  `{1.5, LEN_EM}`; `"12px"` → `{12, LEN_PX}`; `"auto"` → `LEN_AUTO`.
- Unit: resolving `50%` against a 600px container → 300.
- Unit: resolving `1.5em` at 16px base → 24.

## Risk

Medium — interacts with the box model (FEAT-006) and cascade (FEAT-005). Keep
`Length` minimal and well-tested first.
