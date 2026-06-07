# MISS-002 — Image decoding & display

- **Priority:** P2
- **Files:** `network.c`, new `image.{c,h}`, `layout.c`, `render.c`

## Problem

`<img>` produces no box, no fetch, no decode. Pages with meaningful images
(diagrams, figures) lose content. The mission says "lightest/fastest", so images
must be **bounded and optional**.

## Scope (minimal, capped)

1. Decode common web formats: PNG, JPEG, GIF (first frame). Use **SDL2_image**
   if available; otherwise a tiny built-in decoder is out of scope — gate image
   support on SDL2_image at build time (`XS_ENABLE_IMAGES`).
2. Fetch image bytes via the existing network layer (size-capped, content-type
   checked).
3. Layout: `<img>` becomes an inline-block box sized by `width`/`height`
   attributes or the decoded intrinsic size, clamped to the content width.
   Lazy: only fetch/decode images that fall within (or near) the viewport.
4. Render: blit the decoded texture; cache decoded textures (bounded LRU). Show
   `alt` text or a placeholder box on failure.

## Memory discipline

- Cap total decoded image bytes (e.g. 64 MB budget, LRU evict).
- Cap per-image dimensions (downscale huge images on decode).
- Skip images entirely with `--no-images` and when `XS_ENABLE_IMAGES` is off.

## Micro-plan

1. Build option + SDL2_image detection in CMake (optional dependency).
2. `image_fetch_decode(url) -> {pixels/texture, w, h}` with caps.
3. Layout box for `<img>` (intrinsic/attr size, clamp).
4. Render blit + decoded-texture LRU cache + alt fallback.
5. Viewport-based lazy fetch (only when box is near visible).

## Acceptance tests

- Unit (layout): `<img width=100 height=50>` yields a 100×50 box (no decode
  needed for sizing when attrs present).
- Unit: alt fallback box is produced when decode fails.
- Integration (local fixture image via `file://`): image decodes to expected
  dimensions; oversized image is downscaled within the cap.

## Risk

Medium. Optional dependency keeps the core build lean. Strict caps prevent the
memory blowups images are notorious for.
