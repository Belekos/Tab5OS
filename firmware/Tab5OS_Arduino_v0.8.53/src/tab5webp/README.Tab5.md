# Local WebP decoder for Tab5OS 0.8.4

Source: official WebM project, libwebp 1.6.0, archive
https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz
SHA-256: e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
Official documentation: https://developers.google.com/speed/webp/docs/compiling

The 30 selected C files contain only the decoder, demultiplexer and their scalar dependencies. No C encoder, assembly, SIMD, decoder thread, tool or precompiled binary is added. Original auxiliary headers remain available. COPYING (BSD license), PATENTS (patent license) and AUTHORS are supplied unchanged.

Local changes: relative include paths; private configuration injected into each C file; WebPSafeMalloc/Calloc/Free redirected to the bounded PSRAM allocator; cooperation/cancellation points in ParseFrame, DecodeImageData/DecodeAlphaData and Huffman groups. Lossless checkpoints are spaced 256 operations apart to limit checking overhead. Decoding, filtering, scaling and conversion algorithms are not replaced. `tab5_config.h` and `src/webp/config.h` do not change any global SDK, compiler or M5GFX option.

All decoder allocations pass through Tab5WebPPort.cpp: PSRAM is required on ESP32, with a cumulative 12 MiB limit and no internal-heap fallback. This budget includes allocation headers, the encoded file (2 MiB maximum), libwebp working memory, downscaled RGBA and final RGB565. Source dimensions are limited to 4096 per axis and 4,194,304 pixels; output respects the caller's limit and an absolute maximum of 1,048,576 pixels. An image within the dimension limits may still be rejected if decoding exceeds the working-memory budget.

The native decoder downscales before RGBA output. Transparency is composited onto white. An animation displays only its first complete frame at its canvas position; no animation, video playback or timer is started. The demultiplexer validates the container and that frame's position. The port serializes calls to isolate the budget and cooperation checkpoints.

Initial P4 audit (-Os, separate sections): largest stack frame among called decoding functions = 1056 bytes (VP8LBuildHuffmanTable). Shared encoding utilities GetColorPalette and PaletteSortModifiedZeng are not called by the decoder and are discarded by --gc-sections. Cumulative stack use and hardware execution still require measurement on Tab5.
