# FEAT-022 — Background fetch thread (responsiveness)

- **Priority:** P3 (post-correctness; nice-to-have)
- **Files:** `network.c`, `render.c`

## Why

Fetching is synchronous on the UI thread: during a slow load the window freezes
(no scroll, no repaint, no cancel). Moving fetch off-thread keeps the UI live
and lets us show a real loading indicator and support cancel.

## Design

1. A single worker thread (SDL_Thread) owns the curl transfer. The UI thread
   posts a "load URL" request; the worker fetches and posts a completion event
   (`SDL_PushEvent` with a user event carrying the `FetchResult*`).
2. The UI thread, on the completion event, builds the Document, lays out, and
   swaps in the new page — all on the UI thread (parsing/layout stay
   single-threaded; only I/O is off-thread).
3. Cancel: a new navigation supersedes an in-flight one (the worker checks a
   generation counter via `CURLOPT_XFERINFOFUNCTION` progress callback and
   aborts stale transfers).
4. Loading indicator + Esc-to-cancel wired to the status line (FEAT-007).

## Constraints

- Only one outstanding fetch at a time initially (simple, correct). External CSS
  (FEAT-014) can remain synchronous within the worker, or be a follow-up.
- All DOM/layout/render stays on the UI thread → no shared mutable state besides
  the handoff queue, which is mutex-guarded.

## Micro-plan

1. Define a thread-safe single-slot request/response mailbox (mutex + cond, or
   SDL atomics + event queue).
2. Worker loop: wait for request → fetch → push completion event.
3. UI: replace blocking `navigate_to` with "post request + show loading";
   handle completion event to finish the swap.
4. Generation counter for cancel/supersede.

## Acceptance tests

- Unit: mailbox hands off a request and a response without data races (TSan if
  available; otherwise a deterministic single-producer/consumer test).
- Manual: UI scrolls/repaints during a slow load; Esc cancels.

## Risk

Medium–high (threading). Strictly post-correctness. Keep the threading surface
tiny (one worker, one in-flight request) and TSan-checked.
