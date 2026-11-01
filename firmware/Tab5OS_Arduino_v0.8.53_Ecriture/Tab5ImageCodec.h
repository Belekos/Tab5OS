#pragma once
#include "tools/native-apps/image-editor/image_core.h"

namespace Tab5ImageCodec {
struct ImportInfo {
  unsigned sourceWidth = 0, sourceHeight = 0, width = 0, height = 0, reduction = 1;
};
struct Options {
  bool (*cancelled)(void*) = nullptr;
  void* context = nullptr;
  ImportInfo* imported = nullptr;
};
// Readers start at byte zero. Imports keep the original pixel dimensions;
// documents exceeding the 8 MiB pixel budget fail without changing the current
// image or history. JPEG: baseline/grayscale, source <=16 MiB, <=8192 per side.
int loadJpeg(IEDoc*, IERead, void*, Options = {});
// PNG: all standard depths, palettes, alpha and Adam7 interlace; <=16 MiB.
int loadPng(IEDoc*, IERead, void*, Options = {});
// WebP: lossy/lossless RGBA; first frame of animations; encoded source <=2 MiB.
int loadWebp(IEDoc*, IERead, void*, Options = {});
// Flatten visible layers over white, stream RGB scanlines (no subsampling).
// All working storage uses the document allocator. No implicit file overwrite.
int saveJpeg(const IEDoc*, IEWrite, void*, int quality = 92, Options = {});
}
