# Features — Add or Complete Capabilities

Each `FEAT-xxx` either completes a half-built capability or adds a new one that
is a natural extension of the existing design. Entire absent subsystems (images,
tables, JS DOM, encoding) are tracked in
[`../missing/`](../missing/README.md).

## Index

| ID | Pri | Title | Area |
|----|-----|-------|------|
| [FEAT-001](FEAT-001-shared-build-document.md) | P1 | Single `build_document` pipeline | wiring |
| [FEAT-002](FEAT-002-text-align.md) | P1 | Apply `text-align` in layout/render | layout |
| [FEAT-003](FEAT-003-cli-flags.md) | P2 | CLI flags: `--dump`, `--width`, `file://` | main |
| [FEAT-004](FEAT-004-css-selectors.md) | P2 | Class/id/group/universal selectors | css/select |
| [FEAT-005](FEAT-005-typed-cascade.md) | P2 | Typed cascade w/ specificity + inherit | style |
| [FEAT-006](FEAT-006-box-model.md) | P2 | Real margin/padding/border box model | layout |
| [FEAT-007](FEAT-007-ui-chrome.md) | P2 | Scrollbar, title, status, themes | render/ui |
| [FEAT-008](FEAT-008-find-in-page.md) | P3 | Find-in-page (`Ctrl+F`) | render/ui |
| [FEAT-009](FEAT-009-reload-and-caret.md) | P2 | Reload key, URL-bar caret/editing | render/ui |
| [FEAT-010](FEAT-010-interned-tags.md) | P2 | Interned tag enum + parent pointers | dom |
| [FEAT-011](FEAT-011-layout-word-break.md) | P1 | Move word breaking into layout | layout/text |
| [FEAT-012](FEAT-012-link-colors-visited.md) | P3 | Visited-link state + hover cursor | render |
| [FEAT-013](FEAT-013-inline-style-attr.md) | P2 | Inline `style=""` attribute support | css/parser |
| [FEAT-014](FEAT-014-external-css.md) | P2 | `<link rel=stylesheet>` external CSS | css/network |
| [FEAT-015](FEAT-015-default-ua-stylesheet.md) | P2 | Built-in user-agent stylesheet | style |
| [FEAT-020](FEAT-020-network-hardening.md) | P1 | Caps, status, content-type, reuse | network |
| [FEAT-021](FEAT-021-local-fixtures.md) | P1 | `file://` loading + test fixtures | network/main |
| [FEAT-022](FEAT-022-async-fetch.md) | P3 | Background fetch thread | network/render |

## Working rule

Features land behind tests. A feature that cannot be exercised headlessly needs
at least a logic-level unit test plus a render smoke test.
