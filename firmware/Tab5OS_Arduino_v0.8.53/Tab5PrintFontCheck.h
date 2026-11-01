#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace tab5print {
struct FontCheck {
  int offset = 0;
  const char* error = nullptr;
  explicit operator bool() const { return error == nullptr; }
};
namespace fontcheck {
struct Table { size_t offset = 0, size = 0; };
inline uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
inline uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
inline bool range(size_t at, size_t bytes, size_t limit) { return at <= limit && bytes <= limit - at; }
class Check {
  const uint8_t* data;
  size_t size;
  void (*cooperate)();
  unsigned glyphs = 0, longLoca = 0;
  Table loca, glyf;
  size_t glyphOffset(unsigned glyph) const {
    const uint8_t* p = data + loca.offset + glyph * (longLoca ? 4 : 2);
    return longLoca ? u32(p) : size_t(u16(p)) * 2;
  }
  bool simple(size_t at, size_t end, unsigned contours) const {
    if (!contours) return true;
    if (!range(at + 10, size_t(contours) * 2 + 2, end)) return false;
    size_t points = 0;
    for (unsigned i = 0; i < contours; ++i) {
      const size_t next = size_t(u16(data + at + 10 + i * 2)) + 1;
      if (next <= points) return false;
      points = next;
    }
    size_t p = at + 10 + contours * 2;
    const size_t instructions = u16(data + p); p += 2;
    if (!range(p, instructions, end)) return false;
    p += instructions;
    size_t coordinates = 0, seen = 0;
    while (seen < points) {
      if (!range(p, 1, end)) return false;
      const uint8_t flag = data[p++]; size_t repeat = 1;
      if (flag & 8) { if (!range(p, 1, end)) return false; repeat += data[p++]; }
      if (repeat > points - seen) return false;
      const unsigned x = flag & 2 ? 1 : flag & 16 ? 0 : 2;
      const unsigned y = flag & 4 ? 1 : flag & 32 ? 0 : 2;
      coordinates += repeat * (x + y); seen += repeat;
    }
    return range(p, coordinates, end);
  }
  const char* outlines() const {
    // Iterative depth-first walk: no recursive validator and no heap. The
    // rasterizer's unsupported point-matching composites are rejected here.
    struct Frame { unsigned glyph; size_t next, end; bool more; } path[32];
    size_t work = 0;
    const size_t workLimit = size * 4 + size_t(glyphs) * 16;
    for (unsigned root = 0; root < glyphs; ++root) {
      if (cooperate && !(root & 63)) cooperate();
      unsigned depth = 0, current = root;
      for (;;) {
        if (cooperate && !(work & 1023)) cooperate();
        if (++work > workLimit) return "Police trop complexe pour le moteur d’impression";
        if (current != UINT32_MAX) {
          for (unsigned i = 0; i < depth; ++i)
            if (path[i].glyph == current) return "Police invalide : cycle de glyphes";
          size_t at = glyphOffset(current), end = glyphOffset(current + 1);
          if (at != end) {
            at += glyf.offset; end += glyf.offset;
            if (!range(at, 10, end)) return "Police tronquée : glyphe";
            if (int16_t(u16(data + at + 2)) > int16_t(u16(data + at + 6)) ||
                int16_t(u16(data + at + 4)) > int16_t(u16(data + at + 8)))
              return "Police invalide : dimensions de glyphe";
            const int contours = int16_t(u16(data + at));
            if (contours >= 0) {
              if (!simple(at, end, unsigned(contours))) return "Police tronquée : contours";
            } else {
              if (contours != -1 || depth == 32) return "Police composite non prise en charge";
              path[depth++] = {current, at + 10, end, true};
            }
          }
          current = UINT32_MAX;
        }
        while (depth && !path[depth - 1].more) --depth;
        if (!depth) break;
        auto& frame = path[depth - 1]; size_t p = frame.next;
        if (!range(p, 4, frame.end)) return "Police tronquée : composant";
        const unsigned flags = u16(data + p), child = u16(data + p + 2); p += 4;
        if (!(flags & 2)) return "Police : assemblage par points non pris en charge";
        if (child >= glyphs) return "Police invalide : index de composant";
        const unsigned transforms = ((flags & 8) != 0) + ((flags & 64) != 0) + ((flags & 128) != 0);
        if (transforms > 1) return "Police invalide : transformation";
        const size_t bytes = (flags & 1 ? 4 : 2) + (flags & 8 ? 2 : flags & 64 ? 4 : flags & 128 ? 8 : 0);
        if (!range(p, bytes, frame.end)) return "Police tronquée : transformation";
        p += bytes; frame.more = flags & 32; frame.next = p;
        if (!frame.more && (flags & 256)) {
          if (!range(p, 2, frame.end) || !range(p + 2, u16(data + p), frame.end))
            return "Police tronquée : instructions";
        }
        current = child;
      }
    }
    return nullptr;
  }
  const char* mapping(const Table& cmap) const {
    const uint8_t* p = data + cmap.offset;
    if (cmap.size < 4 || !range(4, size_t(u16(p + 2)) * 8, cmap.size)) return "Police tronquée : cmap";
    size_t selected = SIZE_MAX;
    for (unsigned i = 0; i < u16(p + 2); ++i) {
      const uint8_t* r = p + 4 + i * 8;
      if (u16(r) == 0 || (u16(r) == 3 && (u16(r + 2) == 1 || u16(r + 2) == 10))) selected = u32(r + 4);
    }
    if (selected == SIZE_MAX || !range(selected, 4, cmap.size)) return "Police sans table Unicode utilisable";
    p += selected; const unsigned format = u16(p); size_t length;
    if (format == 12 || format == 13) {
      if (!range(selected, 16, cmap.size)) return "Police tronquée : cmap Unicode";
      length = u32(p + 4);
    } else length = u16(p + 2);
    if (!range(selected, length, cmap.size)) return "Police tronquée : cmap Unicode";
    if (format == 0) {
      if (length < 262) return "Police tronquée : cmap 0";
      for (size_t i = 6; i < length; ++i) if (p[i] >= glyphs) return "Police invalide : index Unicode";
    } else if (format == 6) {
      if (length < 10 || !range(10, size_t(u16(p + 8)) * 2, length)) return "Police tronquée : cmap 6";
      for (unsigned i = 0; i < u16(p + 8); ++i) if (u16(p + 10 + i * 2) >= glyphs) return "Police invalide : index Unicode";
    } else if (format == 4) {
      if (length < 16 || (u16(p + 6) & 1)) return "Police invalide : cmap 4";
      const unsigned segments = u16(p + 6) / 2;
      if (!segments || !range(16, size_t(segments) * 8, length)) return "Police tronquée : segments Unicode";
      unsigned power = 1, exponent = 0; while (power * 2 <= segments) { power *= 2; ++exponent; }
      if (u16(p + 8) != power * 2 || u16(p + 10) != exponent || u16(p + 12) != (segments - power) * 2)
        return "Police invalide : recherche Unicode";
      unsigned previous = 0;
      for (unsigned i = 0; i < segments; ++i) {
        const unsigned end = u16(p + 14 + i * 2), start = u16(p + 16 + segments * 2 + i * 2);
        if (start > end || (i && start <= previous)) return "Police invalide : segments Unicode";
        previous = end;
        const uint16_t delta = u16(p + 16 + segments * 4 + i * 2);
        const size_t base = 16 + segments * 6 + i * 2, offset = u16(p + base);
        if (offset && !range(base + offset, size_t(end - start + 1) * 2, length)) return "Police tronquée : index Unicode";
        for (unsigned c = start; c <= end; ++c) {
          const unsigned glyph = offset ? u16(p + base + offset + (c - start) * 2) : uint16_t(c + delta);
          if (glyph >= glyphs) return "Police invalide : index Unicode";
        }
      }
      if (previous != 0xffff) return "Police invalide : fin de cmap";
    } else if (format == 12 || format == 13) {
      if (length < 16 || u32(p + 12) > (length - 16) / 12) return "Police tronquée : groupes Unicode";
      uint32_t previous = 0;
      for (uint32_t i = 0; i < u32(p + 12); ++i) {
        const uint8_t* group = p + 16 + size_t(i) * 12;
        const uint32_t start = u32(group), end = u32(group + 4), glyph = u32(group + 8);
        if (start > end || end > 0x10ffff || (i && start <= previous) || glyph >= glyphs ||
            (format == 12 && end - start >= glyphs - glyph)) return "Police invalide : groupe Unicode";
        previous = end;
      }
    } else return "Police : table Unicode non prise en charge";
    return nullptr;
  }
 public:
  Check(const uint8_t* bytes, size_t length, void (*yield)()) : data(bytes), size(length), cooperate(yield) {}
  FontCheck run() {
    auto fail = [](const char* reason) { return FontCheck{0, reason}; };
    if (!data || size < 12 || size > 2U * 1024U * 1024U) return fail("Police absente ou tronquée");
    size_t offset = 0;
    if (!memcmp(data, "ttcf", 4)) {
      if (size < 16 || (u32(data + 4) != 0x10000 && u32(data + 4) != 0x20000) || !u32(data + 8) ||
          u32(data + 8) > (size - 12) / 4) return fail("Collection de polices invalide");
      offset = u32(data + 12);
    }
    if (!range(offset, 12, size)) return fail("Police tronquée : en-tête");
    if (u32(data + offset) != 0x10000 && memcmp(data + offset, "true", 4))
      return fail("Police d’impression : utiliser un fichier TrueType TTF");
    const unsigned count = u16(data + offset + 4);
    if (!count || !range(offset + 12, size_t(count) * 16, size)) return fail("Police tronquée : tables");
    Table head, hhea, hmtx, maxp, cmap;
    for (unsigned i = 0; i < count; ++i) {
      const uint8_t* r = data + offset + 12 + i * 16;
      const Table table{u32(r + 8), u32(r + 12)};
      if (!range(table.offset, table.size, size)) return fail("Police tronquée : limites de table");
      auto take = [&](const char* tag, Table& dest) { if (!memcmp(r, tag, 4) && !dest.offset) dest = table; };
      take("head", head); take("hhea", hhea); take("hmtx", hmtx); take("maxp", maxp);
      take("cmap", cmap); take("loca", loca); take("glyf", glyf);
    }
    if (!head.offset || head.size < 54 || !hhea.offset || hhea.size < 36 || !maxp.offset || maxp.size < 6 ||
        !hmtx.offset || !cmap.offset || !loca.offset || !glyf.offset) return fail("Police TrueType incomplète");
    glyphs = u16(data + maxp.offset + 4); longLoca = u16(data + head.offset + 50);
    const unsigned metrics = u16(data + hhea.offset + 34), units = u16(data + head.offset + 18);
    if (!glyphs || !metrics || metrics > glyphs || units < 64 || units > 16384 || longLoca > 1 ||
        hmtx.size < size_t(metrics) * 4 + size_t(glyphs - metrics) * 2 ||
        loca.size < size_t(glyphs + 1) * (longLoca ? 4 : 2)) return fail("Police invalide : métriques");
    size_t previous = 0;
    for (unsigned i = 0; i <= glyphs; ++i) {
      const size_t at = glyphOffset(i);
      if (at < previous || at > glyf.size) return fail("Police invalide : offsets de glyphes");
      previous = at;
    }
    if (const char* error = mapping(cmap)) return fail(error);
    if (const char* error = outlines()) return fail(error);
    return {int(offset), nullptr};
  }
};
} // namespace fontcheck
// Bounds/format checks for the TrueType paths this printer uses. This is not a
// general OpenType validator. Call once on the exact, immutable font allocation
// before giving it to stb_truetype; no pointers outlive that allocation.
inline FontCheck checkTrueType(const uint8_t* data, size_t size, void (*cooperate)() = nullptr) {
  return fontcheck::Check(data, size, cooperate).run();
}
} // namespace tab5print
