# MISS-008 — Cookies & HTTP cache

- **Priority:** P3
- **Files:** `network.c`

## Problem

No cookie handling and no HTTP caching. Many sites need a session cookie to
render correctly (or to avoid an interstitial), and refetching identical
resources (CSS, images) on every navigation wastes bandwidth and time.

## Scope (minimal, in-memory, session-only)

1. **Cookies:** enable curl's in-memory cookie engine
   (`CURLOPT_COOKIEFILE, ""`) so cookies are accepted and resent within the
   session. **No disk persistence** (privacy + simplicity). Respect
   Secure/HttpOnly as curl does.
2. **HTTP cache:** a small in-memory LRU keyed by URL storing
   `(body, etag, last-modified, expires)`; on refetch, send
   `If-None-Match`/`If-Modified-Since` and reuse on `304`. Bounded by a byte
   budget.

## Privacy stance

- Session-only by default; cleared on exit. No third-party cookie policy
  engine (out of scope) — document this limitation.
- A `--no-cookies` flag to disable entirely.

## Micro-plan

1. Enable curl cookie engine in `network_init`.
2. Implement the in-memory response cache (LRU + byte cap) with conditional
   request headers.
3. Wire the cache into the fetch path (check before transfer; store after).

## Acceptance tests

- Unit: LRU cache eviction respects the byte budget; conditional headers are
  emitted when an entry exists.
- Integration (local server, guarded): a `304` path reuses the cached body.

## Risk

Low–medium. Cookies via curl are simple; the cache needs careful invalidation
but is bounded and session-scoped.
