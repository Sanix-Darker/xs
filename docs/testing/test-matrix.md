# Test Matrix

Every work item maps to at least one automated test. "Golden" = full-pipeline
`--dump` comparison; "Unit" = headless logic test; "Smoke" = binary runs without
crashing; "San" = clean under ASan/UBSan/LeakSan.

| Item | Unit | Golden | Smoke | San | Notes |
|------|:----:|:------:|:-----:|:---:|-------|
| FIX-000 harness | — | — | ✓ | — | bootstraps everything |
| FIX-001 unknown tag | ✓ | ✓ | ✓ | ✓ | svg/custom element names |
| FIX-002 script text | ✓ | — | ✓ | — | script source round-trip |
| FIX-003 whitespace | ✓ | ✓ | — | — | collapse + pre |
| FIX-004 isspace UB | ✓ | — | — | ✓ | UBSan proves it |
| FIX-005 alloc checks | ✓ | — | — | ✓ | injected-failure allocator |
| FIX-006 css comments | ✓ | — | — | — | desync cases |
| FIX-007 css units | ✓ | ✓ | — | — | %, em, px resolve |
| FIX-008 text measure | ✓ | ✓ | — | — | true-size measurement |
| FIX-009 ownership | ✓ | — | — | ✓ | relayout/free loop |
| FIX-010 http status | ✓ | — | ✓ | — | content-type/charset parse |
| FIX-011 tex cache key | ✓ | — | — | — | content hash + evict |
| FIX-012 path trunc | ✓ | — | — | — | candidate builder |
| FIX-013 italic/bold | ✓ | — | ✓ | — | font key + style flags |
| FIX-014 dead duk | — | — | ✓ | — | build still passes |
| FIX-015 input bounds | ✓ | — | ✓ | ✓ | deep nest, size cap |
| FEAT-001 build_document | ✓ | ✓ | ✓ | — | both entry points equal |
| FEAT-002 text-align | ✓ | ✓ | — | — | center/right shift |
| FEAT-003 cli flags | ✓ | ✓ | ✓ | — | --dump/--width/--version |
| FEAT-004 selectors | ✓ | ✓ | — | — | class/id/group/descendant |
| FEAT-005 cascade | ✓ | ✓ | — | — | specificity/inherit/origin |
| FEAT-006 box model | ✓ | ✓ | — | — | shorthand, collapse |
| FEAT-007 ui chrome | ✓ | — | ✓ | — | scrollbar/theme math |
| FEAT-008 find | ✓ | — | ✓ | — | match + scroll |
| FEAT-009 reload/caret | ✓ | — | ✓ | — | text-edit ops |
| FEAT-010 interned tags | ✓ | — | — | ✓ | intern identity, parents |
| FEAT-011 layout break | ✓ | ✓ | ✓ | ✓ | iterator + node count |
| FEAT-012 visited/hover | ✓ | — | — | — | visited set, hit-test |
| FEAT-013 inline style | ✓ | ✓ | — | — | style attr cascade |
| FEAT-014 external css | ✓ | ✓ | ✓ | — | link collect + order |
| FEAT-015 ua stylesheet | ✓ | ✓ | — | — | origin precedence |
| FEAT-020 net hardening | ✓ | — | ✓ | ✓ | caps, growth, reuse |
| FEAT-021 file fixtures | ✓ | ✓ | ✓ | — | file:// load |
| FEAT-022 async fetch | ✓ | — | ✓ | ✓ | mailbox handoff (TSan) |
| MISS-001 attributes | ✓ | — | — | ✓ | dom_attr/class/id |
| MISS-002 images | ✓ | ✓ | ✓ | ✓ | sizing, alt, caps |
| MISS-003 js bindings | ✓ | — | ✓ | — | console/getElementById |
| MISS-004 forms | ✓ | ✓ | ✓ | — | query-string, focus |
| MISS-005 tables | ✓ | ✓ | ✓ | — | grid model, widths |
| MISS-006 encoding | ✓ | ✓ | — | ✓ | transcode + detect |
| MISS-007 arena | ✓ | — | — | ✓ | align/destroy/accounting |
| MISS-008 cookies/cache | ✓ | — | ✓ | — | LRU + conditional req |
| MISS-009 a11y | ✓ | — | ✓ | — | zoom/contrast/focus order |

## Fixture → item coverage

| Fixture | Exercises |
|---------|-----------|
| basic.html | FEAT-001/003/021, baseline golden |
| whitespace.html | FIX-003, FEAT-011 |
| script.html | FIX-002, MISS-003 |
| css_basic.html | FEAT-004/005/015 |
| css_inline.html | FEAT-013 |
| unknown_tags.html | FIX-001 |
| nested.html | FIX-015 |
| entities.html | MISS-006, parser entities |
| align.html | FEAT-002 |
| table.html | MISS-005 |
| img.html | MISS-002 |
