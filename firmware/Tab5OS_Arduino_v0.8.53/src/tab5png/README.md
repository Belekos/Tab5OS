# Tab5 PNG decoder patch

Derived from M5GFX 0.2.30 `lgfx_pngle.c/.h` (MIT, copyright retained). Symbols are prefixed `tab5_`, and the unchanged SDK miniz implementation is linked. No SDK/library upgrade.

The upstream inflater loop exits when input is exhausted, even when miniz returns `TINFL_STATUS_HAS_MORE_OUTPUT`. Highly compressed 256×256 images can therefore lose their final row. This local copy drains pending output, restores the Adler check disabled in SDK miniz, requires the completed zlib stream and exactly completed rows, rejects excess rows, invalid palette indices and illegal filter bytes, and allocates decoder state in PSRAM. Allocation failure returns -3 separately from malformed data.

Map acceptance additionally validates every PNG chunk CRC/order and full JPEG decoding in `Tab5TileValidation.cpp`. Uniform ocean and fully transparent images are valid; no visual-blank heuristic is used.
