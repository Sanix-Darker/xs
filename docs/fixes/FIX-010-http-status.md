# FIX-010 — HTTP status and content-type not surfaced

- **Severity:** P1 (wrong handling of errors and non-HTML)
- **File:** `network.c`

## Problem

`fetch_url` returns only the response body as a `char*`. It discards:

- **HTTP status code** — a 404/500 page body is treated identically to a 200.
  The user sees the server's error HTML with no indication, and a redirect loop
  or auth wall is indistinguishable from success.
- **Content-Type / charset** — needed to (a) avoid trying to "parse" a binary
  payload (image/pdf/zip) as HTML, and (b) decode non-UTF-8 text (MISS-006).
- **Final URL after redirects** — relative link resolution uses the original
  URL, which is wrong after a cross-origin redirect.

## Evidence

`network.c fetch_url` returns `chunk.data`; no `curl_easy_getinfo` calls.

## Micro-plan

1. Change the API to fill a struct:
   ```c
   typedef struct {
       char*  body;          // owned, NUL-terminated
       size_t length;
       long   status;        // HTTP status (0 if N/A, e.g. file://)
       char   content_type[128];
       char   charset[32];
       char   final_url[2048];
       int    ok;            // CURLE_OK and status < 400
   } FetchResult;
   int fetch_url(const char* url, FetchResult* out);
   void fetch_result_free(FetchResult* r);
   ```
2. After `curl_easy_perform`, pull `CURLINFO_RESPONSE_CODE`,
   `CURLINFO_CONTENT_TYPE`, `CURLINFO_EFFECTIVE_URL`. Parse `charset=` out of the
   content-type.
3. Callers: if `!ok`, show an error document including the status. If
   content-type is non-text and non-HTML, show a "cannot display" page rather
   than parsing binary.
4. Use `final_url` as the base for relative URL resolution.

## Acceptance tests

- Unit (parse helper): `parse_content_type("text/html; charset=ISO-8859-1")`
  yields type `text/html`, charset `iso-8859-1`.
- Integration (httpbin or local server, optional/guarded): a 404 produces the
  error document path; a `image/png` URL is not parsed as HTML.

## Risk

Low–medium. API change ripples to `main.c` and `render.c reload_page`; both go
through the shared `build_document`.
