# FEAT-020 — Network hardening: caps, status, content-type, reuse

- **Priority:** P1
- **Files:** `network.{c,h}`

## Why

`network.c` is a minimal blocking fetch with no size cap (OOM vector, FIX-015),
no status/content-type (FIX-010), linear realloc (O(n²)), and a fresh curl
handle per request (no connection/TLS reuse → slow navigation). This is both a
safety and a speed issue.

## Scope (consolidates FIX-010 + FIX-015 network parts)

1. **FetchResult struct** (see FIX-010): body, length, status, content_type,
   charset, final_url, ok.
2. **Size cap:** `XS_MAX_DOCUMENT_BYTES`; abort transfer past the cap.
3. **Geometric buffer growth:** grow capacity ×2 (or ×1.5) instead of by exact
   chunk size.
4. **Handle reuse:** keep a persistent `CURL*` (curl handle reuse keeps
   connections/TLS sessions warm). Reset per request with `curl_easy_reset` +
   re-set common options.
5. **Sane defaults:** keep UA, follow redirects (cap 5), gzip, timeouts;
   add `CURLOPT_ACCEPT_ENCODING`, `CURLOPT_NOSIGNAL` (thread-safety),
   connect timeout, and `CURLOPT_FAILONERROR` off (we want the body of error
   pages, but we read the status ourselves).
6. **Security:** keep TLS verification ON (default). Document that disabling is
   not offered. Treat all bodies as untrusted.

## Micro-plan

1. Define `FetchResult` and the new `fetch_url(url, out)` signature.
2. Implement the persistent handle in `network_init`/`network_cleanup`.
3. Implement caps + geometric growth in the write callback.
4. Extract status/content-type/charset/final-url via `curl_easy_getinfo`.
5. Update callers via the shared `build_document`/`document_from_fetch`.

## Acceptance tests

- Unit: `parse_content_type` helper (type + charset extraction) — table of
  cases.
- Unit: write-callback growth respects the cap (inject a tiny cap, feed more,
  assert truncation/abort and no overflow).
- Integration (guarded/local server): status 404 path; non-HTML content-type
  path; gzip body decoded.

## Risk

Medium. API change is wide but funnels through `build_document`. Handle reuse
must `reset` correctly to avoid stale options leaking between requests.
