# Missing — Absent Subsystems

Each `MISS-xxx` is a capability the browser lacks **entirely** (not a half-built
feature). These are larger than individual features and several are explicitly
scoped down to fit the "minimal, fast, low-memory" mission.

## Index

| ID | Pri | Title | Scope note |
|----|-----|-------|-----------|
| [MISS-001](MISS-001-attributes.md) | P1 | Generic attribute capture | foundation for many features |
| [MISS-002](MISS-002-images.md) | P2 | Image decoding & display | `<img>`, capped |
| [MISS-003](MISS-003-js-dom-bindings.md) | P2 | JS host bindings (console/document) | minimal, read-mostly |
| [MISS-004](MISS-004-forms.md) | P3 | Forms & basic input | GET forms, text inputs |
| [MISS-005](MISS-005-tables.md) | P2 | Table layout | simple grid, no spanning first |
| [MISS-006](MISS-006-encoding.md) | P1 | Character encoding detection | UTF-8 + Latin-1 + meta charset |
| [MISS-007](MISS-007-arena-allocator.md) | P1 | Arena allocator for DOM/strings | core memory architecture |
| [MISS-008](MISS-008-cookies-cache.md) | P3 | Cookies & HTTP cache | session cookies, in-mem cache |
| [MISS-009](MISS-009-accessibility.md) | P3 | Accessibility affordances | scalable text, high contrast |

## Working rule

Each missing subsystem ships in the smallest correct increment, fully tested,
before any enhancement. Scope creep is the enemy of the mission.
