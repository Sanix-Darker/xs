# FEAT-015 — Built-in user-agent stylesheet

- **Priority:** P2 (replaces magic constants; baseline for cascade)
- **Files:** new `ua_css.h` (embedded string), `style.c`

## Why

Layout spacing is hardcoded in `layout.c` (`PARAGRAPH_SPACING`,
`HEADING_MARGIN_*`, `LIST_INDENT`, font sizes per heading). A real browser
expresses these as a **user-agent stylesheet** — the lowest-priority origin in
the cascade. Centralizing defaults in CSS makes them overridable by author CSS
(correct cascade) and removes scattered magic numbers.

## Design

Embed a compact UA stylesheet as a C string, parsed once at startup into a
stylesheet with **UA origin** (lowest priority). Example contents:

```css
html,body { display:block; color:#1e1e1e; font-size:16px; }
h1{font-size:28px;font-weight:bold;margin:24px 0 12px}
h2{font-size:24px;font-weight:bold;margin:22px 0 11px}
/* ... h3..h6 ... */
p{margin:8px 0}
ul,ol{margin:8px 0;padding-left:25px}
li{display:list-item}
blockquote{margin:10px 0;padding-left:30px;border-left:3px solid #a0a0a0}
pre,code{font-family:mono;font-size:14px;background:#f0eeeb}
a{color:#1446b4;text-decoration:underline}
b,strong{font-weight:bold}
i,em{font-style:italic}
hr{margin:10px 0}
div,section,article,nav,header,footer,main,aside{display:block}
```

(Values chosen to match current visual output so this is a refactor, not a
redesign.)

## Micro-plan

1. Author the UA CSS string to reproduce current spacing/sizes.
2. Parse it once into a UA-origin stylesheet at startup; cache it.
3. Cascade order: UA < author(external, inline `<style>`) < inline `style=` attr
   < `!important`.
4. Remove the corresponding magic constants from `layout.c` as the box model
   (FEAT-006) consumes the UA margins. Stage: keep constants until parity proven
   via goldens, then delete.

## Acceptance tests

- Unit: an `<h1>` with no author CSS gets font-size 28 and bold from the UA
  sheet.
- Unit: author `h1{font-size:40px}` overrides the UA 28 (origin precedence).
- Golden `--dump`: a fixture's spacing matches the pre-refactor baseline within
  rounding.

## Risk

Medium. Tied to cascade (FEAT-005) and box model (FEAT-006). The "match current
output" constraint keeps it a controlled refactor.
