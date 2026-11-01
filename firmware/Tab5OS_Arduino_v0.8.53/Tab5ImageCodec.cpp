#include "Tab5ImageCodec.h"
#include "src/jpeg/jpge.h"
#include "src/tab5png/tab5_pngle.h"
#include <lgfx/utility/lgfx_tjpgd.h>
#include <algorithm>
#include <cstring>
#include <new>

namespace Tab5ImageCodec {
namespace {
constexpr size_t MaxSource = 16u * 1024u * 1024u;
constexpr unsigned MaxSourceSide = 8192;
constexpr size_t DecoderPoolBytes = 4096;

struct Memory {
  const IEDoc& doc;
  void* data;
  Memory(const IEDoc& d, size_t bytes) : doc(d), data(d.alloc(d.memory_context, bytes)) {}
  ~Memory() { if (data) doc.release(doc.memory_context, data); }
  Memory(const Memory&) = delete;
  Memory& operator=(const Memory&) = delete;
};
bool cooperate(const IEDoc& doc, const Options& options) {
  if (doc.yield) doc.yield(doc.yield_context);
  return !options.cancelled || !options.cancelled(options.context);
}
int fail(IEDoc* doc, const char* error) { if (doc) doc->error = error; return 0; }

struct ReadContext {
  IEDoc& original;
  IEDoc* image;
  IERead reader;
  void* input;
  Options options;
  size_t bytes = 0, pixels = 0;
  uint8_t previous = 0, last = 0;
  bool cancelled = false, invalid = false, tooLarge = false;

  bool step() {
    if (!cooperate(original, options)) cancelled = true;
    return !cancelled && !invalid && !tooLarge;
  }
  size_t take(uint8_t* out, size_t count) {
    if (!step()) return 0;
    if (count > MaxSource - bytes) count = MaxSource - bytes;
    if (!count) {
      // A file exactly at the limit is legal; distinguish EOF from an extra
      // byte without allocating or reading an unbounded tail.
      uint8_t extra;
      if (reader(input, &extra, 1)) tooLarge = true;
      return 0;
    }
    const size_t got = reader(input, out, count);
    if (got > count) { invalid = true; return 0; }
    if (got) {
      previous = got > 1 ? out[got - 2] : last;
      last = out[got - 1]; bytes += got;
    }
    return got;
  }
  static uint32_t read(void* opaque, uint8_t* output, uint32_t count) {
    auto& c = *static_cast<ReadContext*>(opaque);
    uint8_t skip[256]; uint32_t total = 0;
    while (total < count) {
      const size_t want = output ? count - total : std::min<size_t>(sizeof(skip), count - total);
      const size_t got = c.take(output ? output + total : skip, want);
      if (!got) break;
      total += uint32_t(got);
    }
    return total;
  }
  bool finish() {
    uint8_t tail[256];
    // The decoder may finish its last MCU before requesting EOI. Drain the
    // sequential input to establish the byte limit and reject truncated files.
    while (bytes < MaxSource) {
      if (!step()) return false;
      const size_t got = take(tail, std::min(sizeof(tail), MaxSource - bytes));
      if (!got) return !invalid && !cancelled && previous == 0xff && last == 0xd9;
    }
    if (!step()) return false;
    const size_t extra = reader(input, tail, 1);
    if (extra) { tooLarge = true; return false; }
    return previous == 0xff && last == 0xd9;
  }
};

uint32_t decodedBlock(void* opaque, void* bitmap, JRECT* rect) {
  auto& c = *static_cast<ReadContext*>(opaque);
  if (!c.step()) return 0;
  if (!c.image || !bitmap || !rect || rect->right < rect->left || rect->bottom < rect->top ||
      rect->right >= unsigned(c.image->current.width) || rect->bottom >= unsigned(c.image->current.height)) {
    c.invalid = true; return 0;
  }
  const auto* rgb = static_cast<const uint8_t*>(bitmap);
  for (unsigned y = rect->top; y <= rect->bottom; ++y) {
    auto* out = c.image->current.layers[0].pixels + size_t(y) * c.image->current.width + rect->left;
    for (unsigned x = rect->left; x <= rect->right; ++x, rgb += 3)
      *out++ = IE_RGBA(rgb[0], rgb[1], rgb[2], 255);
  }
  c.pixels += size_t(rect->right - rect->left + 1) * (rect->bottom - rect->top + 1);
  return 1;
}

void decodedPng(void* opaque, uint32_t x, uint32_t y, uint_fast8_t step, size_t count, const uint8_t* argb) {
  auto& c = *static_cast<ReadContext*>(opaque);
  if (!c.step()) return;
  if (!c.image || !argb || !step || !count || y >= unsigned(c.image->current.height) ||
      x >= unsigned(c.image->current.width) || count - 1 > (unsigned(c.image->current.width) - 1 - x) / step) {
    c.invalid = true; return;
  }
  auto* row = c.image->current.layers[0].pixels + size_t(y) * c.image->current.width;
  for (size_t i = 0; i < count; ++i, x += step, argb += 4)
    row[x] = IE_RGBA(argb[1], argb[2], argb[3], argb[0]);
  c.pixels += count;
}

class Output final : public jpge::output_stream {
 public:
  const IEDoc& doc;
  IEWrite writer;
  void* context;
  Options options;
  size_t bytes = 0;
  Output(const IEDoc& d, IEWrite w, void* c, Options o) : doc(d), writer(w), context(c), options(o) {}
  bool put_buf(const void* data, int length) override {
    if (length < 0 || (!data && length)) return false;
    const auto* p = static_cast<const uint8_t*>(data);
    size_t left = size_t(length);
    if (left > MaxSource - bytes) return false;
    while (left) {
      if (!cooperate(doc, options)) return false;
      const size_t want = std::min<size_t>(left, 4096), got = writer(context, p, want);
      if (!got || got > want) return false;
      bytes += got; p += got; left -= got;
    }
    return true;
  }
};
}

int loadJpeg(IEDoc* doc, IERead reader, void* input, Options options) {
  if (options.imported) *options.imported = ImportInfo{};
  if (!doc || !doc->alloc || !doc->release || !reader) return fail(doc, "Lecture JPEG impossible.");
  if (!cooperate(*doc, options)) return fail(doc, "Import JPEG annule; image conservee.");
  Memory pool(*doc, DecoderPoolBytes);
  if (!pool.data) return fail(doc, "Memoire insuffisante pour le decodeur JPEG.");
  lgfxJdec decoder{};
  ReadContext context{*doc, nullptr, reader, input, options};
  const auto prepared = lgfx_jd_prepare(&decoder, ReadContext::read, pool.data, DecoderPoolBytes, &context);
  if (prepared != JDR_OK) return fail(doc, context.cancelled ? "Import JPEG annule; image conservee." :
      context.tooLarge ? "JPEG trop volumineux (16 Mio maximum)." :
      prepared == JDR_FMT3 ? "JPEG non pris en charge (utiliser un JPEG standard)." : "JPEG incomplet ou invalide; image conservee.");
  const unsigned sw = decoder.width, sh = decoder.height;
  if (!sw || !sh || sw > MaxSourceSide || sh > MaxSourceSide)
    return fail(doc, "Dimensions JPEG invalides (8192 maximum).");
  const unsigned w = sw, h = sh;
  if (w > IE_MAX_DIMENSION || h > IE_MAX_DIMENSION || size_t(w) > IE_DOCUMENT_MAX_BYTES / 4 / h)
    return fail(doc, "Dimensions/calques hors limite (8192, moins de 8 Mio de pixels).");
  IEDoc temporary{};
  if (!ie_init(&temporary, doc->alloc, doc->release, doc->memory_context, int(w), int(h))) {
    ie_destroy(&temporary);
    return fail(doc, "Memoire insuffisante; image conservee.");
  }
  ie_set_yield(&temporary, doc->yield, doc->yield_context);
  context.image = &temporary;
  const auto result = lgfx_jd_decomp(&decoder, decodedBlock, 0);
  const bool ok = result == JDR_OK && !context.invalid && !context.cancelled &&
                  context.pixels == size_t(w) * h && context.finish();
  if (!ok) {
    ie_destroy(&temporary);
    return fail(doc, context.cancelled ? "Import JPEG annule; image conservee." :
        context.tooLarge ? "JPEG trop volumineux (16 Mio maximum)." : "JPEG incomplet ou invalide; image conservee.");
  }
  std::strcpy(temporary.current.layers[0].name, "Image JPEG");
  temporary.dirty = 0; temporary.error = nullptr;
  // The clipboard belongs to the editor, not to the imported image. Move
  // ownership only after successful decoding; a failed import keeps both.
  if (!ie_clipboard_transfer(&temporary, doc)) {
    ie_destroy(&temporary);
    return fail(doc, "Import impossible; image conservee.");
  }
  ie_destroy(doc); *doc = temporary;
  if (options.imported) *options.imported = {sw, sh, w, h, 1};
  return 1;
}

int loadPng(IEDoc* doc, IERead reader, void* input, Options options) {
  if (options.imported) *options.imported = ImportInfo{};
  if (!doc || !doc->alloc || !doc->release || !reader) return fail(doc, "Lecture PNG impossible.");
  if (!cooperate(*doc, options)) return fail(doc, "Import PNG annule; image conservee.");
  tab5_pngle_t* decoder = tab5_pngle_new_with_allocator(doc->alloc, doc->release, doc->memory_context);
  if (!decoder) return fail(doc, "Memoire insuffisante pour le decodeur PNG.");
  ReadContext context{*doc, nullptr, reader, input, options};
  IEDoc temporary{};
  const int prepared = tab5_pngle_prepare(decoder, ReadContext::read, &context);
  const unsigned w = tab5_pngle_get_width(decoder), h = tab5_pngle_get_height(decoder);
  bool ok = false;
  const char* error = "PNG incomplet ou invalide; image conservee.";
  if (prepared == -3) error = "Memoire insuffisante pour le decodeur PNG.";
  if (prepared == 0) {
    if (!w || !h || w > IE_MAX_DIMENSION || h > IE_MAX_DIMENSION || size_t(w) > IE_DOCUMENT_MAX_BYTES / 4 / h)
      error = "Dimensions/calques hors limite (8192, moins de 8 Mio de pixels).";
    else if (!ie_init(&temporary, doc->alloc, doc->release, doc->memory_context, int(w), int(h)))
      error = "Memoire insuffisante; image conservee.";
    else {
      ie_set_yield(&temporary, doc->yield, doc->yield_context); context.image = &temporary;
      ok = tab5_pngle_decomp(decoder, decodedPng) == 0 && !context.invalid && !context.cancelled &&
           !context.tooLarge && context.pixels == size_t(w) * h;
      // The IEND was checked by the decoder. Bound any remaining encoded tail.
      uint8_t tail[256];
      while (ok && context.take(tail, sizeof(tail))) {}
      ok = ok && !context.invalid && !context.cancelled && !context.tooLarge;
    }
  }
  tab5_pngle_destroy(decoder);
  if (!ok) {
    ie_destroy(&temporary);
    return fail(doc, context.cancelled ? "Import PNG annule; image conservee." :
        context.tooLarge ? "PNG trop volumineux (16 Mio maximum)." : error);
  }
  std::strcpy(temporary.current.layers[0].name, "Image PNG");
  temporary.dirty = 0; temporary.error = nullptr;
  // The clipboard belongs to the editor, not to the imported image. Move
  // ownership only after successful decoding; a failed import keeps both.
  if (!ie_clipboard_transfer(&temporary, doc)) {
    ie_destroy(&temporary);
    return fail(doc, "Import impossible; image conservee.");
  }
  ie_destroy(doc); *doc = temporary;
  if (options.imported) *options.imported = {w, h, w, h, 1};
  return 1;
}

int saveJpeg(const IEDoc* doc, IEWrite writer, void* output, int quality, Options options) {
  if (!doc || !doc->alloc || !doc->release || !writer || quality < 1 || quality > 100 ||
      doc->current.width < 1 || doc->current.height < 1 || doc->current.width > IE_MAX_DIMENSION ||
      doc->current.height > IE_MAX_DIMENSION || doc->current.count < 1 || doc->current.count > IE_MAX_LAYERS ||
      !cooperate(*doc, options)) return 0;
  Memory storage(*doc, sizeof(jpge::jpeg_encoder)), scanline(*doc, size_t(doc->current.width) * 3);
  if (!storage.data || !scanline.data) return 0;
  auto* encoder = new (storage.data) jpge::jpeg_encoder;
  encoder->set_allocator(doc->alloc, doc->release, doc->memory_context);
  Output stream(*doc, writer, output, options);
  jpge::params parameters; parameters.m_quality = quality; parameters.m_subsampling = jpge::H1V1;
  bool ok = encoder->init(&stream, doc->current.width, doc->current.height, 3, parameters);
  auto* row = static_cast<uint8_t*>(scanline.data);
  for (int y = 0; ok && y < doc->current.height; ++y) {
    if (!cooperate(*doc, options)) { ok = false; break; }
    for (int x = 0; x < doc->current.width; ++x) {
      const uint32_t pixel = ie_composite(doc, x, y);
      const unsigned alpha = pixel >> 24, white = 255 * (255 - alpha);
      row[x * 3] = uint8_t(((pixel & 255) * alpha + white + 127) / 255);
      row[x * 3 + 1] = uint8_t((((pixel >> 8) & 255) * alpha + white + 127) / 255);
      row[x * 3 + 2] = uint8_t((((pixel >> 16) & 255) * alpha + white + 127) / 255);
    }
    ok = encoder->process_scanline(row);
  }
  if (ok) ok = encoder->process_scanline(nullptr) && cooperate(*doc, options);
  encoder->~jpeg_encoder();
  return ok ? 1 : 0;
}
}
