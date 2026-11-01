#pragma once

#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>

// Self-contained raw DEFLATE decoder below: no external ZIP/decompression library.

namespace epub_lite {

static constexpr size_t MAX_XML_BYTES = 768 * 1024;
static constexpr size_t MAX_CHAPTER_HTML_BYTES = 3 * 1024 * 1024;
static constexpr uint16_t MAX_MANIFEST_ITEMS = 768;
static constexpr uint16_t MAX_CHAPTERS = 256;
static constexpr size_t EPUB_PATH_CAP = 256;
static constexpr size_t EPUB_LABEL_CAP = 128;

struct ZipEntry {
  uint32_t localOffset = 0;
  uint32_t compressedSize = 0;
  uint32_t uncompressedSize = 0;
  uint32_t crc32 = 0;
  uint16_t method = 0;
  uint16_t flags = 0;
};

struct ManifestItem {
  char id[72];
  char href[EPUB_PATH_CAP];
  char mediaType[64];
  bool nav = false;
  bool cover = false;
};

struct Book {
  String title;
  String author;
  String opfPath;
  String coverPath;
  char (*chapters)[EPUB_PATH_CAP] = nullptr;
  char (*sectionLabels)[EPUB_LABEL_CAP] = nullptr;
  uint16_t chapterCount = 0;
  uint32_t chapterBytes[MAX_CHAPTERS] = {};
  uint32_t contentFingerprint = 0;
  bool fixedLayout = false;
  int defaultViewportW = 0;
  int defaultViewportH = 0;

  void clear() {
    if (chapters) heap_caps_free(chapters);
    if (sectionLabels) heap_caps_free(sectionLabels);
    chapters = nullptr;
    sectionLabels = nullptr;
    chapterCount = 0;
    memset(chapterBytes, 0, sizeof(chapterBytes));
    contentFingerprint = 0;
    title = "";
    author = "";
    opfPath = "";
    coverPath = "";
    fixedLayout = false;
    defaultViewportW = 0;
    defaultViewportH = 0;
  }
};

static inline uint16_t rd16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

static inline uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

static String xmlEntityDecode(String s) {
  s.replace("&amp;", "&");
  s.replace("&quot;", "\"");
  s.replace("&apos;", "'");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&#39;", "'");
  return s;
}

static String parentPath(const String& p) {
  int slash = p.lastIndexOf('/');
  return (slash < 0) ? String("") : p.substring(0, slash + 1);
}

static String normalizePath(String p) {
  p.replace('\\', '/');
  while (p.startsWith("/")) p.remove(0, 1);
  while (p.indexOf("//") >= 0) p.replace("//", "/");

  String out;
  int start = 0;
  while (start <= static_cast<int>(p.length())) {
    int slash = p.indexOf('/', start);
    if (slash < 0) slash = p.length();
    String part = p.substring(start, slash);
    if (part == "..") {
      int prev = out.lastIndexOf('/');
      if (prev >= 0) out.remove(prev);
      else out = "";
    } else if (part.length() && part != ".") {
      if (out.length()) out += '/';
      out += part;
    }
    if (slash >= static_cast<int>(p.length())) break;
    start = slash + 1;
  }
  return out;
}

static int fromHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static String urlDecodePath(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); ++i) {
    if (in[i] == '%' && i + 2 < in.length()) {
      int a = fromHex(in[i + 1]);
      int b = fromHex(in[i + 2]);
      if (a >= 0 && b >= 0) {
        out += static_cast<char>((a << 4) | b);
        i += 2;
        continue;
      }
    }
    out += in[i];
  }
  return out;
}

static bool findEocd(File& f, uint32_t& centralOffset, uint16_t& entryCount, String& err) {
  const uint32_t size = static_cast<uint32_t>(f.size());
  if (size < 22) { err = "EPUB/ZIP trop petit"; return false; }

  const uint32_t scanLen = (size > 65557U) ? 65557U : size;
  uint8_t* tail = static_cast<uint8_t*>(heap_caps_malloc(scanLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!tail) { err = "Mémoire insuffisante pour index ZIP"; return false; }
  if (!f.seek(size - scanLen) || f.read(tail, scanLen) != scanLen) {
    heap_caps_free(tail); err = "Lecture index ZIP impossible"; return false;
  }

  int32_t pos = static_cast<int32_t>(scanLen) - 22;
  for (; pos >= 0; --pos) {
    if (rd32(tail + pos) == 0x06054B50UL) break;
  }
  if (pos < 0) {
    heap_caps_free(tail); err = "Fin de répertoire ZIP introuvable"; return false;
  }

  const uint8_t* p = tail + pos;
  entryCount = rd16(p + 10);
  centralOffset = rd32(p + 16);
  const uint32_t centralSize = rd32(p + 12);
  heap_caps_free(tail);

  if (entryCount == 0xFFFF || centralOffset == 0xFFFFFFFFUL || centralSize == 0xFFFFFFFFUL) {
    err = "EPUB ZIP64 non pris en charge";
    return false;
  }
  return true;
}

static bool readCentralEntry(File& f, String& name, ZipEntry& e) {
  uint8_t h[46];
  if (f.read(h, sizeof(h)) != sizeof(h)) return false;
  if (rd32(h) != 0x02014B50UL) return false;

  e.flags = rd16(h + 8);
  e.method = rd16(h + 10);
  e.crc32 = rd32(h + 16);
  e.compressedSize = rd32(h + 20);
  e.uncompressedSize = rd32(h + 24);
  const uint16_t nameLen = rd16(h + 28);
  const uint16_t extraLen = rd16(h + 30);
  const uint16_t commentLen = rd16(h + 32);
  e.localOffset = rd32(h + 42);

  if (nameLen == 0 || nameLen >= EPUB_PATH_CAP) {
    if (!f.seek(f.position() + nameLen + extraLen + commentLen)) return false;
    name = "";
    return true;
  }

  char buf[EPUB_PATH_CAP];
  if (f.read(reinterpret_cast<uint8_t*>(buf), nameLen) != nameLen) return false;
  buf[nameLen] = 0;
  name = normalizePath(String(buf));
  if (extraLen + commentLen && !f.seek(f.position() + extraLen + commentLen)) return false;
  return true;
}

static bool locateEntry(fs::FS& fs, const char* archivePath, const String& wanted, ZipEntry& out, String& err) {
  File f = fs.open(archivePath, FILE_READ);
  if (!f) { err = "EPUB inaccessible"; return false; }

  uint32_t centralOffset = 0;
  uint16_t count = 0;
  if (!findEocd(f, centralOffset, count, err)) { f.close(); return false; }
  if (!f.seek(centralOffset)) { f.close(); err = "Index ZIP invalide"; return false; }

  const String target = normalizePath(wanted);
  const String decodedTarget = urlDecodePath(target);
  for (uint16_t i = 0; i < count; ++i) {
    String name;
    ZipEntry e;
    if (!readCentralEntry(f, name, e)) break;
    if (name.length() && (name == target || name == decodedTarget || urlDecodePath(name) == decodedTarget)) {
      out = e;
      f.close();
      return true;
    }
  }
  f.close();
  err = "Entrée EPUB introuvable: " + wanted;
  return false;
}


// -----------------------------------------------------------------------------
// Minimal RFC1951 raw-DEFLATE inflater.
// EPUB ZIP entries use raw DEFLATE streams (method 8).
// This implementation supports stored, fixed-Huffman and dynamic-Huffman blocks.
// -----------------------------------------------------------------------------

struct DeflateBits {
  const uint8_t* src = nullptr;
  size_t size = 0;
  size_t pos = 0;
  uint32_t hold = 0;
  uint8_t bits = 0;
  bool ok = true;

  uint32_t get(unsigned n) {
    while (bits < n) {
      if (pos >= size) { ok = false; return 0; }
      hold |= static_cast<uint32_t>(src[pos++]) << bits;
      bits += 8;
    }
    uint32_t v = hold & ((n == 32) ? 0xFFFFFFFFUL : ((1UL << n) - 1UL));
    hold >>= n;
    bits -= n;
    return v;
  }

  void alignByte() {
    unsigned drop = bits & 7U;
    hold >>= drop;
    bits -= drop;
  }
};

struct DeflateHuffman {
  uint16_t count[16] = {0};
  uint16_t symbol[320] = {0};
};

static bool buildDeflateHuffman(const uint8_t* lengths, unsigned n, DeflateHuffman& h) {
  memset(&h, 0, sizeof(h));
  for (unsigned i = 0; i < n; ++i) {
    if (lengths[i] > 15) return false;
    ++h.count[lengths[i]];
  }
  if (h.count[0] == n) return false;

  int left = 1;
  for (unsigned len = 1; len <= 15; ++len) {
    left <<= 1;
    left -= h.count[len];
    if (left < 0) return false; // over-subscribed tree
  }

  uint16_t offs[16] = {0};
  offs[1] = 0;
  for (unsigned len = 1; len < 15; ++len) offs[len + 1] = offs[len] + h.count[len];
  for (unsigned sym = 0; sym < n; ++sym) {
    uint8_t len = lengths[sym];
    if (len) h.symbol[offs[len]++] = sym;
  }
  return true;
}

static int decodeDeflateSymbol(DeflateBits& b, const DeflateHuffman& h) {
  int code = 0;
  int first = 0;
  int index = 0;
  for (int len = 1; len <= 15; ++len) {
    code |= static_cast<int>(b.get(1));
    if (!b.ok) return -1;
    int count = h.count[len];
    if (code - count < first) {
      int at = index + (code - first);
      if (at < 0 || at >= 320) return -1;
      return h.symbol[at];
    }
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  return -1;
}

static bool inflateCompressedBlock(DeflateBits& b, const DeflateHuffman& litlen,
                                   const DeflateHuffman& dist,
                                   uint8_t* out, size_t outCap, size_t& outPos) {
  static const uint16_t lenBase[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,
    131,163,195,227,258
  };
  static const uint8_t lenExtra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
  };
  static const uint16_t distBase[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
    1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
  };
  static const uint8_t distExtra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
  };

  for (;;) {
    int sym = decodeDeflateSymbol(b, litlen);
    if (sym < 0) return false;
    if (sym < 256) {
      if (outPos >= outCap) return false;
      out[outPos++] = static_cast<uint8_t>(sym);
      continue;
    }
    if (sym == 256) return true;
    if (sym < 257 || sym > 285) return false;

    unsigned li = static_cast<unsigned>(sym - 257);
    size_t length = lenBase[li] + b.get(lenExtra[li]);
    if (!b.ok) return false;

    int dsym = decodeDeflateSymbol(b, dist);
    if (dsym < 0 || dsym > 29) return false;
    size_t distance = distBase[dsym] + b.get(distExtra[dsym]);
    if (!b.ok || distance == 0 || distance > outPos || outPos + length > outCap) return false;

    size_t from = outPos - distance;
    for (size_t i = 0; i < length; ++i) out[outPos++] = out[from + i];
  }
}

static bool inflateRawDeflate(const uint8_t* src, size_t srcLen, uint8_t* out,
                              size_t outCap, size_t& written) {
  DeflateBits b;
  b.src = src; b.size = srcLen;
  written = 0;
  bool finalBlock = false;

  while (!finalBlock && b.ok) {
    finalBlock = b.get(1) != 0;
    unsigned type = b.get(2);
    if (!b.ok || type == 3) return false;

    if (type == 0) {
      b.alignByte();
      uint16_t len = static_cast<uint16_t>(b.get(16));
      uint16_t nlen = static_cast<uint16_t>(b.get(16));
      if (!b.ok || static_cast<uint16_t>(~len) != nlen || written + len > outCap) return false;
      for (uint16_t i = 0; i < len; ++i) {
        out[written++] = static_cast<uint8_t>(b.get(8));
        if (!b.ok) return false;
      }
      continue;
    }

    uint8_t litLengths[288] = {0};
    uint8_t distLengths[32] = {0};

    if (type == 1) {
      for (unsigned i = 0; i <= 143; ++i) litLengths[i] = 8;
      for (unsigned i = 144; i <= 255; ++i) litLengths[i] = 9;
      for (unsigned i = 256; i <= 279; ++i) litLengths[i] = 7;
      for (unsigned i = 280; i <= 287; ++i) litLengths[i] = 8;
      for (unsigned i = 0; i < 32; ++i) distLengths[i] = 5;
    } else {
      unsigned hlit = b.get(5) + 257;
      unsigned hdist = b.get(5) + 1;
      unsigned hclen = b.get(4) + 4;
      if (!b.ok || hlit > 286 || hdist > 32) return false;

      static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
      uint8_t codeLengths[19] = {0};
      for (unsigned i = 0; i < hclen; ++i) codeLengths[order[i]] = static_cast<uint8_t>(b.get(3));
      if (!b.ok) return false;

      DeflateHuffman codeTree;
      if (!buildDeflateHuffman(codeLengths, 19, codeTree)) return false;

      uint8_t lengths[318] = {0};
      unsigned total = hlit + hdist;
      unsigned idx = 0;
      while (idx < total) {
        int sym = decodeDeflateSymbol(b, codeTree);
        if (sym < 0) return false;
        if (sym <= 15) {
          lengths[idx++] = static_cast<uint8_t>(sym);
        } else if (sym == 16) {
          if (idx == 0) return false;
          unsigned repeat = b.get(2) + 3;
          if (!b.ok || idx + repeat > total) return false;
          uint8_t prev = lengths[idx - 1];
          while (repeat--) lengths[idx++] = prev;
        } else if (sym == 17) {
          unsigned repeat = b.get(3) + 3;
          if (!b.ok || idx + repeat > total) return false;
          while (repeat--) lengths[idx++] = 0;
        } else if (sym == 18) {
          unsigned repeat = b.get(7) + 11;
          if (!b.ok || idx + repeat > total) return false;
          while (repeat--) lengths[idx++] = 0;
        } else return false;
      }
      memcpy(litLengths, lengths, hlit);
      memcpy(distLengths, lengths + hlit, hdist);
      if (litLengths[256] == 0) return false;
    }

    DeflateHuffman litTree, distTree;
    if (!buildDeflateHuffman(litLengths, 288, litTree)) return false;
    // A distance tree can be represented by a single non-zero symbol.
    if (!buildDeflateHuffman(distLengths, 32, distTree)) return false;
    if (!inflateCompressedBlock(b, litTree, distTree, out, outCap, written)) return false;
  }
  return b.ok;
}

static bool extractEntry(fs::FS& fs, const char* archivePath, const String& entryName,
                         uint8_t** outData, size_t* outLen, size_t maxBytes, String& err) {
  *outData = nullptr;
  *outLen = 0;
  ZipEntry e;
  if (!locateEntry(fs, archivePath, entryName, e, err)) return false;
  if (e.flags & 0x0001) { err = "EPUB chiffré non pris en charge"; return false; }
  if (e.uncompressedSize > maxBytes) { err = "Chapitre EPUB trop volumineux"; return false; }
  if (e.uncompressedSize == 0) {
    uint8_t* empty = static_cast<uint8_t*>(heap_caps_malloc(1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!empty) { err = "Mémoire insuffisante"; return false; }
    empty[0] = 0; *outData = empty; *outLen = 0; return true;
  }

  File f = fs.open(archivePath, FILE_READ);
  if (!f || !f.seek(e.localOffset)) { if (f) f.close(); err = "Lecture chapitre impossible"; return false; }
  uint8_t lh[30];
  if (f.read(lh, sizeof(lh)) != sizeof(lh) || rd32(lh) != 0x04034B50UL) {
    f.close(); err = "En-tête ZIP local invalide"; return false;
  }
  const uint16_t nameLen = rd16(lh + 26);
  const uint16_t extraLen = rd16(lh + 28);
  const uint32_t dataPos = e.localOffset + 30U + nameLen + extraLen;
  if (!f.seek(dataPos)) { f.close(); err = "Offset ZIP invalide"; return false; }

  uint8_t* out = static_cast<uint8_t*>(heap_caps_malloc(static_cast<size_t>(e.uncompressedSize) + 1,
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!out) { f.close(); err = "Mémoire PSRAM insuffisante"; return false; }

  if (e.method == 0) {
    const size_t got = f.read(out, e.uncompressedSize);
    f.close();
    if (got != e.uncompressedSize) { heap_caps_free(out); err = "Lecture EPUB incomplète"; return false; }
  } else if (e.method == 8) {
    uint8_t* comp = static_cast<uint8_t*>(heap_caps_malloc(e.compressedSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!comp) { f.close(); heap_caps_free(out); err = "Mémoire compression insuffisante"; return false; }
    const size_t gotComp = f.read(comp, e.compressedSize);
    f.close();
    if (gotComp != e.compressedSize) {
      heap_caps_free(comp); heap_caps_free(out); err = "Lecture ZIP compressée incomplète"; return false;
    }
    size_t got = 0;
    bool ok = inflateRawDeflate(comp, e.compressedSize, out, e.uncompressedSize, got);
    heap_caps_free(comp);
    if (!ok || got != e.uncompressedSize) {
      heap_caps_free(out); err = "Décompression Deflate EPUB impossible"; return false;
    }
  } else {
    f.close(); heap_caps_free(out); err = "Compression EPUB non prise en charge"; return false;
  }

  out[e.uncompressedSize] = 0;
  *outData = out;
  *outLen = e.uncompressedSize;
  return true;
}

// Public lightweight resource extractor used by the portrait reader for EPUB images.
// Data is allocated in PSRAM and must be released with heap_caps_free().
static bool extractResource(fs::FS& fs, const char* archivePath, const String& entryName,
                            uint8_t** outData, size_t* outLen, size_t maxBytes, String& err) {
  return extractEntry(fs, archivePath, entryName, outData, outLen, maxBytes, err);
}

static bool asciiNameEquals(const String& a, const char* b) {
  if (!b || a.length() != strlen(b)) return false;
  for (size_t i = 0; i < a.length(); ++i) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
    if (ca != cb) return false;
  }
  return true;
}

// XML attribute reader tolerant of arbitrary whitespace around '=' and namespace
// prefixes. This is intentionally small, but much less brittle than indexOf("x=").
static String attrValue(const String& tag, const char* attr) {
  int i = 0;
  const int n = static_cast<int>(tag.length());
  while (i < n && tag[i] != '<') ++i;
  if (i < n) ++i;
  // Skip element name first.
  while (i < n && !isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '>' && tag[i] != '/') ++i;

  while (i < n) {
    while (i < n && isspace(static_cast<unsigned char>(tag[i]))) ++i;
    if (i >= n || tag[i] == '>' || tag[i] == '/') break;

    const int nameStart = i;
    while (i < n && !isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '=' && tag[i] != '>' && tag[i] != '/') ++i;
    String name = tag.substring(nameStart, i);
    int colon = name.lastIndexOf(':');
    if (colon >= 0) name = name.substring(colon + 1);

    while (i < n && isspace(static_cast<unsigned char>(tag[i]))) ++i;
    if (i >= n || tag[i] != '=') {
      while (i < n && tag[i] != '>' && !isspace(static_cast<unsigned char>(tag[i]))) ++i;
      continue;
    }
    ++i;
    while (i < n && isspace(static_cast<unsigned char>(tag[i]))) ++i;
    if (i >= n) break;

    const char q = tag[i];
    if (q != '\'' && q != '"') {
      while (i < n && !isspace(static_cast<unsigned char>(tag[i])) && tag[i] != '>') ++i;
      continue;
    }
    const int valueStart = ++i;
    while (i < n && tag[i] != q) ++i;
    if (i >= n) break;
    if (asciiNameEquals(name, attr)) return xmlEntityDecode(tag.substring(valueStart, i));
    ++i;
  }
  return "";
}

static int findStartTagByLocalName(const String& xml, const char* localName, int from = 0) {
  const int n = static_cast<int>(xml.length());
  int p = from;
  while (p < n) {
    p = xml.indexOf('<', p);
    if (p < 0 || p + 1 >= n) return -1;
    char c = xml[p + 1];
    if (c == '/' || c == '!' || c == '?') { p += 2; continue; }

    int q = p + 1;
    while (q < n && !isspace(static_cast<unsigned char>(xml[q])) && xml[q] != '>' && xml[q] != '/') ++q;
    String name = xml.substring(p + 1, q);
    int colon = name.lastIndexOf(':');
    if (colon >= 0) name = name.substring(colon + 1);
    if (asciiNameEquals(name, localName)) return p;
    p = q;
  }
  return -1;
}

static bool findFallbackOpf(fs::FS& fs, const char* archivePath, String& opf, String& err) {
  File f = fs.open(archivePath, FILE_READ);
  if (!f) { err = "EPUB inaccessible"; return false; }
  uint32_t centralOffset = 0;
  uint16_t count = 0;
  if (!findEocd(f, centralOffset, count, err) || !f.seek(centralOffset)) { f.close(); return false; }

  String firstOpf;
  String preferred;
  for (uint16_t i = 0; i < count; ++i) {
    String name; ZipEntry z;
    if (!readCentralEntry(f, name, z)) break;
    String low = name; low.toLowerCase();
    if (!low.endsWith(".opf")) continue;
    if (!firstOpf.length()) firstOpf = name;
    int slash = low.lastIndexOf('/');
    String base = slash >= 0 ? low.substring(slash + 1) : low;
    if (base == "package.opf" || base == "content.opf") {
      preferred = name;
      break;
    }
  }
  f.close();
  opf = preferred.length() ? preferred : firstOpf;
  if (!opf.length()) { err = "Aucun fichier OPF trouvé dans l’EPUB"; return false; }
  return true;
}

static String tagText(const String& xml, const char* tagName) {
  String open = "<" + String(tagName);
  int p = xml.indexOf(open);
  if (p < 0) return "";
  p = xml.indexOf('>', p);
  if (p < 0) return "";
  String close = "</" + String(tagName) + ">";
  int e = xml.indexOf(close, p + 1);
  if (e < 0) return "";
  String s = xml.substring(p + 1, e);
  s.trim();
  return xmlEntityDecode(s);
}

static bool isHtmlHref(String href) {
  int cut = -1;
  int hash = href.indexOf('#');
  int query = href.indexOf('?');
  if (hash >= 0) cut = hash;
  if (query >= 0 && (cut < 0 || query < cut)) cut = query;
  if (cut >= 0) href.remove(cut);
  href.toLowerCase();
  return href.endsWith(".xhtml") || href.endsWith(".html") || href.endsWith(".htm");
}

static bool isXhtmlMediaType(String mediaType) {
  mediaType.trim();
  mediaType.toLowerCase();
  int semi = mediaType.indexOf(';');
  if (semi >= 0) mediaType.remove(semi);
  mediaType.trim();
  return mediaType == "application/xhtml+xml" || mediaType == "text/html";
}

static bool propertiesContainToken(String props, const char* token) {
  props.trim();
  int start = 0;
  while (start < static_cast<int>(props.length())) {
    while (start < static_cast<int>(props.length()) && isspace(static_cast<unsigned char>(props[start]))) ++start;
    int end = start;
    while (end < static_cast<int>(props.length()) && !isspace(static_cast<unsigned char>(props[end]))) ++end;
    String part = props.substring(start, end);
    if (asciiNameEquals(part, token)) return true;
    start = end + 1;
  }
  return false;
}

static String metaTextByProperty(const String& xml, const char* wantedProperty) {
  int pos = 0;
  while (true) {
    int p = findStartTagByLocalName(xml, "meta", pos);
    if (p < 0) return "";
    int e = xml.indexOf('>', p);
    if (e < 0) return "";
    String tag = xml.substring(p, e + 1);
    String prop = attrValue(tag, "property");
    if (asciiNameEquals(prop, wantedProperty)) {
      int close = xml.indexOf("</meta", e + 1);
      if (close >= 0) {
        String value = xml.substring(e + 1, close);
        value.trim();
        return xmlEntityDecode(value);
      }
    }
    pos = e + 1;
  }
}

static String metaContentByName(const String& xml, const char* wantedName) {
  int pos = 0;
  while (true) {
    int p = findStartTagByLocalName(xml, "meta", pos);
    if (p < 0) return "";
    int e = xml.indexOf('>', p);
    if (e < 0) return "";
    String tag = xml.substring(p, e + 1);
    String name = attrValue(tag, "name");
    if (asciiNameEquals(name, wantedName)) return attrValue(tag, "content");
    pos = e + 1;
  }
}

static bool parseResolution(String value, int& w, int& h) {
  value.trim();
  value.replace('X', 'x');
  int x = value.indexOf('x');
  if (x < 1) return false;
  w = value.substring(0, x).toInt();
  h = value.substring(x + 1).toInt();
  return w > 0 && h > 0;
}

static int viewportNumber(String content, const char* key) {
  String low = content;
  low.toLowerCase();
  String needle = String(key);
  needle.toLowerCase();
  int p = low.indexOf(needle);
  if (p < 0) return 0;
  p += needle.length();
  while (p < static_cast<int>(low.length()) && isspace(static_cast<unsigned char>(low[p]))) ++p;
  if (p >= static_cast<int>(low.length()) || low[p] != '=') return 0;
  ++p;
  while (p < static_cast<int>(low.length()) && isspace(static_cast<unsigned char>(low[p]))) ++p;
  int start = p;
  while (p < static_cast<int>(low.length()) && (isdigit(static_cast<unsigned char>(low[p])) || low[p] == '.')) ++p;
  if (p <= start) return 0;
  return static_cast<int>(low.substring(start, p).toFloat() + 0.5f);
}

static bool parseViewportFromHtml(const uint8_t* html, size_t len, int& w, int& h) {
  w = h = 0;
  if (!html || !len) return false;
  const size_t scanLen = len < 32768 ? len : 32768;
  String head;
  head.reserve(scanLen + 1);
  for (size_t i = 0; i < scanLen; ++i) head += static_cast<char>(html[i]);

  int pos = 0;
  while (true) {
    int p = findStartTagByLocalName(head, "meta", pos);
    if (p < 0) break;
    int e = head.indexOf('>', p);
    if (e < 0) break;
    String tag = head.substring(p, e + 1);
    String name = attrValue(tag, "name");
    if (asciiNameEquals(name, "viewport")) {
      String content = attrValue(tag, "content");
      w = viewportNumber(content, "width");
      h = viewportNumber(content, "height");
      if (w > 0 && h > 0) return true;
    }
    pos = e + 1;
  }

  // SVG fixed-layout documents often use viewBox instead of meta viewport.
  int sp = findStartTagByLocalName(head, "svg");
  if (sp >= 0) {
    int se = head.indexOf('>', sp);
    if (se > sp) {
      String tag = head.substring(sp, se + 1);
      String vb = attrValue(tag, "viewBox");
      vb.replace(',', ' ');
      float vals[4] = {0,0,0,0};
      int found = 0;
      int i = 0;
      while (i < static_cast<int>(vb.length()) && found < 4) {
        while (i < static_cast<int>(vb.length()) && isspace(static_cast<unsigned char>(vb[i]))) ++i;
        int start = i;
        while (i < static_cast<int>(vb.length()) && !isspace(static_cast<unsigned char>(vb[i]))) ++i;
        if (i > start) vals[found++] = vb.substring(start, i).toFloat();
      }
      if (found == 4 && vals[2] > 0 && vals[3] > 0) {
        w = static_cast<int>(vals[2] + 0.5f);
        h = static_cast<int>(vals[3] + 0.5f);
        return w > 0 && h > 0;
      }
    }
  }
  return false;
}

static bool isReadableManifestItem(const ManifestItem& item) {
  return isXhtmlMediaType(String(item.mediaType)) || isHtmlHref(String(item.href));
}

static String resolvePackageHref(const String& base, String href) {
  int cut = -1;
  int hash = href.indexOf('#');
  int query = href.indexOf('?');
  if (hash >= 0) cut = hash;
  if (query >= 0 && (cut < 0 || query < cut)) cut = query;
  if (cut >= 0) href.remove(cut);
  href = urlDecodePath(href);
  // URL paths beginning with '/' are rooted at the EPUB container root.
  if (href.startsWith("/")) return normalizePath(href);
  return normalizePath(base + href);
}

// Return a compact human-readable label from XHTML content (e.g. an EPUB nav link).
// Markup inside the anchor is removed and whitespace is normalized.
static String navigationLabelText(const String& inner) {
  String out;
  out.reserve(inner.length());
  bool inTag = false;
  bool pendingSpace = false;
  for (size_t i = 0; i < inner.length(); ++i) {
    char c = inner[i];
    if (c == '<') { inTag = true; continue; }
    if (c == '>') { inTag = false; continue; }
    if (inTag) continue;
    if (isspace(static_cast<unsigned char>(c))) {
      pendingSpace = out.length() > 0;
      continue;
    }
    if (pendingSpace && out.length() && out[out.length() - 1] != ' ') out += ' ';
    pendingSpace = false;
    out += c;
  }
  out.replace("&nbsp;", " ");
  out = xmlEntityDecode(out);
  out.trim();
  while (out.indexOf("  ") >= 0) out.replace("  ", " ");
  return out;
}

static int findEndTagByLocalName(const String& xml, const char* localName, int from = 0) {
  const int n = static_cast<int>(xml.length());
  int p = from;
  while (p < n) {
    p = xml.indexOf("</", p);
    if (p < 0 || p + 2 >= n) return -1;
    int q = p + 2;
    while (q < n && isspace(static_cast<unsigned char>(xml[q]))) ++q;
    int nameStart = q;
    while (q < n && !isspace(static_cast<unsigned char>(xml[q])) && xml[q] != '>') ++q;
    String name = xml.substring(nameStart, q);
    int colon = name.lastIndexOf(':');
    if (colon >= 0) name = name.substring(colon + 1);
    if (asciiNameEquals(name, localName)) return p;
    p = q;
  }
  return -1;
}

// Map labels supplied by the EPUB Navigation Document to the spine entries.
// We intentionally inspect every internal <a href> in the nav document: the TOC
// normally supplies chapter labels, while landmarks often supplies cover/title-page
// labels that are absent from the TOC. First label wins for a given spine document.
static void populateNavigationLabels(fs::FS& fs, const char* archivePath,
                                     const String& navPath, Book& book) {
  if (!navPath.length() || !book.sectionLabels || !book.chapters || !book.chapterCount) return;
  uint8_t* navData = nullptr;
  size_t navLen = 0;
  String navErr;
  if (!extractEntry(fs, archivePath, navPath, &navData, &navLen, MAX_XML_BYTES, navErr)) return;
  String navXml(reinterpret_cast<char*>(navData));
  heap_caps_free(navData);

  const String navBase = parentPath(navPath);
  int pos = 0;
  while (true) {
    int p = findStartTagByLocalName(navXml, "a", pos);
    if (p < 0) break;
    int tagEnd = navXml.indexOf('>', p);
    if (tagEnd < 0) break;
    String tag = navXml.substring(p, tagEnd + 1);
    String href = attrValue(tag, "href");
    int close = findEndTagByLocalName(navXml, "a", tagEnd + 1);
    if (close < 0) { pos = tagEnd + 1; continue; }
    int closeEnd = navXml.indexOf('>', close);
    if (closeEnd < 0) closeEnd = close + 3;

    if (href.length() && !href.startsWith("http:") && !href.startsWith("https:") &&
        !href.startsWith("mailto:") && !href.startsWith("data:")) {
      String label = navigationLabelText(navXml.substring(tagEnd + 1, close));
      String target = resolvePackageHref(navBase, href);
      if (label.length() && target.length()) {
        for (uint16_t i = 0; i < book.chapterCount; ++i) {
          if (book.sectionLabels[i][0]) continue;
          if (target == String(book.chapters[i])) {
            label.toCharArray(book.sectionLabels[i], EPUB_LABEL_CAP);
            break;
          }
        }
      }
    }
    pos = closeEnd + 1;
  }
}

static const char* sectionLabel(const Book& book, uint16_t index) {
  if (!book.sectionLabels || index >= book.chapterCount) return "";
  return book.sectionLabels[index];
}

static bool buildBook(fs::FS& fs, const char* archivePath, Book& book, String& err) {
  book.clear();

  uint8_t* containerData = nullptr;
  size_t containerLen = 0;
  String containerXml;
  String containerErr;
  if (extractEntry(fs, archivePath, "META-INF/container.xml", &containerData, &containerLen, 64 * 1024, containerErr)) {
    containerXml = String(reinterpret_cast<char*>(containerData));
    heap_caps_free(containerData);
  }

  String opf;
  int rf = findStartTagByLocalName(containerXml, "rootfile");
  if (rf >= 0) {
    int rfEnd = containerXml.indexOf('>', rf);
    if (rfEnd >= 0) {
      String rootTag = containerXml.substring(rf, rfEnd + 1);
      opf = attrValue(rootTag, "full-path");
    }
  }

  // A conforming EPUB provides full-path in META-INF/container.xml. Some real
  // files are less clean (odd spacing/prefixes or broken container metadata),
  // so fall back to locating an OPF in the ZIP instead of refusing the book.
  if (opf.length()) opf = normalizePath(urlDecodePath(opf));
  if (!opf.length()) {
    String fallbackErr;
    if (!findFallbackOpf(fs, archivePath, opf, fallbackErr)) {
      if (!containerXml.length() && containerErr.length()) err = containerErr + "; " + fallbackErr;
      else if (rf < 0) err = String("container.xml sans rootfile; ") + fallbackErr;
      else err = String("Chemin OPF absent; ") + fallbackErr;
      return false;
    }
  }
  book.opfPath = opf;

  uint8_t* opfData = nullptr;
  size_t opfLen = 0;
  if (!extractEntry(fs, archivePath, opf, &opfData, &opfLen, MAX_XML_BYTES, err)) {
    // If container.xml points to a bad path, try another OPF present in ZIP.
    String fallback;
    String fallbackErr;
    if (!findFallbackOpf(fs, archivePath, fallback, fallbackErr) || fallback == opf ||
        !extractEntry(fs, archivePath, fallback, &opfData, &opfLen, MAX_XML_BYTES, err)) return false;
    opf = fallback;
    book.opfPath = opf;
  }
  String xml(reinterpret_cast<char*>(opfData));
  heap_caps_free(opfData);

  book.title = tagText(xml, "dc:title");
  if (book.title.length() == 0) book.title = tagText(xml, "title");
  book.author = tagText(xml, "dc:creator");
  if (book.author.length() == 0) book.author = tagText(xml, "creator");

  // EPUB 3 fixed-layout metadata. Reflowable books normally do not define a
  // physical page size, so the reader will use a book-like virtual page.
  String renditionLayout = metaTextByProperty(xml, "rendition:layout");
  renditionLayout.trim(); renditionLayout.toLowerCase();
  book.fixedLayout = (renditionLayout == "pre-paginated");
  String legacyFixed = metaContentByName(xml, "fixed-layout");
  legacyFixed.trim(); legacyFixed.toLowerCase();
  if (legacyFixed == "true" || legacyFixed == "yes" || legacyFixed == "1") book.fixedLayout = true;
  String originalResolution = metaContentByName(xml, "original-resolution");
  parseResolution(originalResolution, book.defaultViewportW, book.defaultViewportH);
  const String legacyCoverId = metaContentByName(xml, "cover");

  ManifestItem* items = static_cast<ManifestItem*>(heap_caps_calloc(MAX_MANIFEST_ITEMS, sizeof(ManifestItem),
                                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!items) { err = "Mémoire manifeste EPUB insuffisante"; return false; }
  uint16_t itemCount = 0;

  int pos = 0;
  while (itemCount < MAX_MANIFEST_ITEMS) {
    int p = findStartTagByLocalName(xml, "item", pos);
    if (p < 0) break;
    int e = xml.indexOf('>', p);
    if (e < 0) break;
    String tag = xml.substring(p, e + 1);
    String id = attrValue(tag, "id");
    String href = attrValue(tag, "href");
    String mediaType = attrValue(tag, "media-type");
    String properties = attrValue(tag, "properties");
    if (id.length() && href.length()) {
      id.toCharArray(items[itemCount].id, sizeof(items[itemCount].id));
      href.toCharArray(items[itemCount].href, sizeof(items[itemCount].href));
      mediaType.toCharArray(items[itemCount].mediaType, sizeof(items[itemCount].mediaType));
      items[itemCount].nav = propertiesContainToken(properties, "nav");
      items[itemCount].cover = propertiesContainToken(properties, "cover-image") ||
                               (legacyCoverId.length() && id == legacyCoverId);
      ++itemCount;
    }
    pos = e + 1;
  }

  book.chapters = static_cast<char (*)[EPUB_PATH_CAP]>(heap_caps_calloc(MAX_CHAPTERS, EPUB_PATH_CAP,
                                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!book.chapters) {
    heap_caps_free(items); err = "Mémoire chapitres EPUB insuffisante"; return false;
  }
  book.sectionLabels = static_cast<char (*)[EPUB_LABEL_CAP]>(
      heap_caps_calloc(MAX_CHAPTERS, EPUB_LABEL_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  // Labels are a navigation enhancement: failure to allocate them must not make
  // an otherwise valid EPUB unreadable.

  const String base = parentPath(opf);
  String navDocPath;
  for (uint16_t i = 0; i < itemCount; ++i) {
    if (!navDocPath.length() && items[i].nav)
      navDocPath = resolvePackageHref(base, String(items[i].href));
    if (!book.coverPath.length() && items[i].cover)
      book.coverPath = resolvePackageHref(base, String(items[i].href));
  }
  pos = 0;
  while (book.chapterCount < MAX_CHAPTERS) {
    int p = findStartTagByLocalName(xml, "itemref", pos);
    if (p < 0) break;
    int e = xml.indexOf('>', p);
    if (e < 0) break;
    String tag = xml.substring(p, e + 1);
    String idref = attrValue(tag, "idref");
    if (idref.length()) {
      for (uint16_t i = 0; i < itemCount; ++i) {
        if (idref == items[i].id) {
          // The EPUB spine defines reading order. Prefer XHTML media-type;
          // tolerate old/simple books whose media-type is missing but href is HTML.
          if (!isReadableManifestItem(items[i])) break;
          String full = resolvePackageHref(base, String(items[i].href));
          full.toCharArray(book.chapters[book.chapterCount], EPUB_PATH_CAP);
          ++book.chapterCount;
          break;
        }
      }
    }
    pos = e + 1;
  }
  // If the spine is absent or unusable, keep the readable manifest order.
  // This also supports valid XHTML content documents whose filenames do not
  // end in .xhtml/.html: EPUB identifies them by media-type.
  if (book.chapterCount == 0) {
    for (uint16_t i = 0; i < itemCount && book.chapterCount < MAX_CHAPTERS; ++i) {
      if (!isReadableManifestItem(items[i]) || items[i].nav) continue;
      String full = resolvePackageHref(base, String(items[i].href));
      full.toCharArray(book.chapters[book.chapterCount], EPUB_PATH_CAP);
      ++book.chapterCount;
    }
  }

  const uint16_t parsedManifestItems = itemCount;
  heap_caps_free(items);

  // Last-resort compatibility fallback for damaged/non-conforming books:
  // enumerate obvious HTML/XHTML files in ZIP order.
  if (book.chapterCount == 0) {
    File f = fs.open(archivePath, FILE_READ);
    if (!f) { err = "EPUB inaccessible"; book.clear(); return false; }
    uint32_t centralOffset = 0; uint16_t count = 0;
    if (!findEocd(f, centralOffset, count, err) || !f.seek(centralOffset)) {
      f.close(); book.clear(); return false;
    }
    for (uint16_t i = 0; i < count && book.chapterCount < MAX_CHAPTERS; ++i) {
      String name; ZipEntry z;
      if (!readCentralEntry(f, name, z)) break;
      String low = name; low.toLowerCase();
      if (isHtmlHref(low) && low.indexOf("nav.xhtml") < 0 && low.indexOf("toc.") < 0) {
        name.toCharArray(book.chapters[book.chapterCount], EPUB_PATH_CAP);
        ++book.chapterCount;
      }
    }
    f.close();
  }

  if (book.chapterCount == 0) {
    err = "Aucun document de lecture EPUB trouvé (manifest=" + String(parsedManifestItems) + ")";
    book.clear();
    return false;
  }

  // Keep the spine for actual page-turn order, but use the EPUB's own navigation
  // labels for what the reader calls each section. This prevents cover/title/about
  // pages from being misreported as Chapter 1/2/3.
  if (book.sectionLabels && navDocPath.length())
    populateNavigationLabels(fs, archivePath, navDocPath, book);
  // One central-directory pass supplies progress weights without inflating every
  // section. Include all ZIP entries (CSS, images, etc.) in cache identity.
  File indexFile = fs.open(archivePath, FILE_READ);
  if (indexFile) {
    uint32_t centralOffset = 0; uint16_t entries = 0;
    String indexError;
    if (findEocd(indexFile, centralOffset, entries, indexError) && indexFile.seek(centralOffset)) {
      uint32_t signature = 2166136261U;
      auto hashByte = [&](uint8_t b) { signature = (signature ^ b) * 16777619U; };
      bool valid = true;
      for (uint16_t i = 0; i < entries; ++i) {
        String name; ZipEntry entry;
        if (!readCentralEntry(indexFile, name, entry)) { valid = false; break; }
        for (size_t n = 0; n < name.length(); ++n) hashByte(static_cast<uint8_t>(name[n]));
        hashByte(0);
        for (uint8_t n = 0; n < 4; ++n) {
          hashByte(static_cast<uint8_t>(entry.crc32 >> (n * 8)));
          hashByte(static_cast<uint8_t>(entry.uncompressedSize >> (n * 8)));
        }
        for (uint16_t chapter = 0; chapter < book.chapterCount; ++chapter)
          if (name == book.chapters[chapter]) book.chapterBytes[chapter] = entry.uncompressedSize;
        if ((i & 31U) == 31U) delay(0);
      }
      if (valid) book.contentFingerprint = signature ? signature : 1;
    }
    indexFile.close();
  }
  return true;
}

static void appendUtf8(uint32_t cp, char* out, size_t cap, size_t& o) {
  auto put = [&](uint8_t b) { if (o + 1 < cap) out[o++] = static_cast<char>(b); };
  if (cp <= 0x7F) put(cp);
  else if (cp <= 0x7FF) {
    put(0xC0 | (cp >> 6)); put(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    put(0xE0 | (cp >> 12)); put(0x80 | ((cp >> 6) & 0x3F)); put(0x80 | (cp & 0x3F));
  } else if (cp <= 0x10FFFF) {
    put(0xF0 | (cp >> 18)); put(0x80 | ((cp >> 12) & 0x3F)); put(0x80 | ((cp >> 6) & 0x3F)); put(0x80 | (cp & 0x3F));
  }
}

static uint32_t namedEntity(const String& e) {
  if (e == "nbsp") return ' ';
  if (e == "amp") return '&';
  if (e == "lt") return '<';
  if (e == "gt") return '>';
  if (e == "quot") return '"';
  if (e == "apos") return '\'';
  if (e == "hellip") return 0x2026;
  if (e == "mdash") return 0x2014;
  if (e == "ndash") return 0x2013;
  if (e == "laquo") return 0x00AB;
  if (e == "raquo") return 0x00BB;
  if (e == "rsquo") return 0x2019;
  if (e == "lsquo") return 0x2018;
  if (e == "ldquo") return 0x201C;
  if (e == "rdquo") return 0x201D;
  if (e == "copy") return 0x00A9;
  return 0;
}

static bool startsBlockTag(String tag) {
  tag.toLowerCase();
  tag.trim();
  if (tag.startsWith("/")) tag.remove(0, 1);
  int sp = tag.indexOf(' '); if (sp >= 0) tag.remove(sp);
  return tag == "p" || tag == "div" || tag == "section" || tag == "article" ||
         tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6" ||
         tag == "blockquote" || tag == "pre" || tag == "li" || tag == "br" || tag == "hr";
}

static bool isImageTag(const String& rawTag) {
  String t = rawTag;
  t.trim();
  if (t.startsWith("/")) return false;
  int sp = t.indexOf(' ');
  int slash = t.indexOf('/');
  int cut = -1;
  if (sp >= 0) cut = sp;
  if (slash >= 0 && (cut < 0 || slash < cut)) cut = slash;
  String name = cut >= 0 ? t.substring(0, cut) : t;
  int colon = name.lastIndexOf(':');
  if (colon >= 0) name = name.substring(colon + 1);
  name.toLowerCase();
  return name == "img" || name == "image";
}

static void appendImageMarker(const String& chapterPath, const String& rawTag,
                              char* out, size_t cap, size_t& o) {
  String wrapped = "<" + rawTag + ">";
  String src = attrValue(wrapped, "src");
  if (!src.length()) src = attrValue(wrapped, "href"); // SVG <image href/xlink:href>
  if (!src.length() || src.startsWith("data:")) return;
  String full = resolvePackageHref(parentPath(chapterPath), src);
  if (!full.length()) return;

  auto put = [&](char c) { if (o + 1 < cap) out[o++] = c; };
  if (o && out[o - 1] != '\n') put('\n');
  put(0x1E); // Record Separator: start image marker
  const char prefix[] = "IMG:";
  for (size_t i = 0; i < sizeof(prefix) - 1; ++i) put(prefix[i]);
  for (size_t i = 0; i < full.length(); ++i) put(full[i]);
  put(0x1F); // Unit Separator: end image marker
  put('\n');
}

// Convert XHTML into a compact reader stream. Text stays UTF-8 and image
// references are preserved as invisible control markers:
//   0x1E "IMG:<archive path>" 0x1F
// The UI paginates text and turns each marker into an image page.
static bool htmlToReaderText(const uint8_t* html, size_t len, const String& chapterPath,
                             char** outText, size_t* outLen, String& err) {
  // Image markers can add path bytes that were not part of visible text.
  const size_t cap = len + 64 * 1024;
  char* out = static_cast<char*>(heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!out) { err = "Mémoire conversion XHTML insuffisante"; return false; }

  size_t o = 0;
  bool inTag = false;
  bool skipScript = false;
  bool skipHead = false;
  bool lastSpace = false;
  int newlineRun = 0;
  String tag;
  tag.reserve(128);

  auto addChar = [&](char c) {
    if (o + 1 < cap) out[o++] = c;
  };
  auto addSpace = [&]() {
    if (o == 0 || lastSpace || newlineRun) return;
    addChar(' '); lastSpace = true;
  };
  auto addNewline = [&]() {
    while (o && out[o - 1] == ' ') --o;
    if (newlineRun < 2 && o + 1 < cap) { out[o++] = '\n'; ++newlineRun; }
    lastSpace = false;
  };

  for (size_t i = 0; i < len; ++i) {
    char c = static_cast<char>(html[i]);
    if (!inTag && c == '<') {
      inTag = true; tag = ""; continue;
    }
    if (inTag) {
      if (c == '>') {
        inTag = false;
        String t = tag; t.trim();
        String low = t; low.toLowerCase();

        // The XHTML <head> is metadata, not book content. Reading <title> here
        // created bogus pages such as "cover" / "title page" before the real cover.
        if (low.startsWith("head") || low.indexOf(":head") >= 0) { skipHead = true; continue; }
        if (low.startsWith("/head") || low.indexOf("/:head") >= 0) { skipHead = false; continue; }
        if (skipHead) continue;

        if (low.startsWith("script") || low.startsWith("style")) skipScript = true;
        if (low.startsWith("/script") || low.startsWith("/style")) { skipScript = false; continue; }
        if (!skipScript && isImageTag(t)) {
          while (o && out[o - 1] == ' ') --o;
          appendImageMarker(chapterPath, t, out, cap, o);
          lastSpace = false;
          newlineRun = 1;
          continue;
        }
        if (!skipScript && startsBlockTag(t)) {
          if (low.startsWith("li")) { addNewline(); appendUtf8(0x2022, out, cap, o); addChar(' '); }
          else addNewline();
        }
        continue;
      }
      if (tag.length() < 256) tag += c;
      continue;
    }
    if (skipScript || skipHead) continue;

    if (c == '&') {
      size_t semi = i + 1;
      while (semi < len && semi - i < 16 && html[semi] != ';') ++semi;
      if (semi < len && html[semi] == ';') {
        char entBuf[16] = {0};
        size_t entLen = semi - i - 1;
        if (entLen >= sizeof(entBuf)) entLen = sizeof(entBuf) - 1;
        memcpy(entBuf, html + i + 1, entLen);
        String ent(entBuf);
        uint32_t cp = 0;
        if (ent.startsWith("#x") || ent.startsWith("#X")) cp = strtoul(ent.c_str() + 2, nullptr, 16);
        else if (ent.startsWith("#")) cp = strtoul(ent.c_str() + 1, nullptr, 10);
        else cp = namedEntity(ent);
        if (cp) {
          if (cp == ' ') addSpace();
          else { appendUtf8(cp, out, cap, o); lastSpace = false; newlineRun = 0; }
          i = semi;
          continue;
        }
      }
    }

    if (c == '\r' || c == '\n' || c == '\t' || c == ' ') addSpace();
    else {
      addChar(c);
      lastSpace = false;
      newlineRun = 0;
    }
  }

  while (o && (out[o - 1] == ' ' || out[o - 1] == '\n')) --o;
  out[o] = 0;
  *outText = out;
  *outLen = o;
  return true;
}

static bool readChapter(fs::FS& fs, const char* archivePath, const Book& book, uint16_t index,
                        char** outText, size_t* outLen, String& err,
                        int* viewportW = nullptr, int* viewportH = nullptr) {
  *outText = nullptr; *outLen = 0;
  if (viewportW) *viewportW = 0;
  if (viewportH) *viewportH = 0;
  if (!book.chapters || index >= book.chapterCount) { err = "Chapitre invalide"; return false; }
  uint8_t* html = nullptr; size_t htmlLen = 0;
  const String chapterPath(book.chapters[index]);
  if (!extractEntry(fs, archivePath, chapterPath, &html, &htmlLen, MAX_CHAPTER_HTML_BYTES, err)) return false;
  int vw = 0, vh = 0;
  parseViewportFromHtml(html, htmlLen, vw, vh);
  if (viewportW) *viewportW = vw;
  if (viewportH) *viewportH = vh;
  const bool ok = htmlToReaderText(html, htmlLen, chapterPath, outText, outLen, err);
  heap_caps_free(html);
  return ok;
}

} // namespace epub_lite

