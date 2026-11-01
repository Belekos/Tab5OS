#pragma once

#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>
#include "EpubLite.h"

// Lightweight EPUB XHTML/CSS parser for Tab5OS/Tab5Writer.
// It intentionally implements a static, reading-oriented subset of HTML/CSS:
// headings, paragraphs, rich inline text, lists, blockquotes, images, HR and
// simple tables. The output is a compact display list stored in PSRAM.

namespace epub_render {

static inline int iMax(int a, int b) { return a > b ? a : b; }
static inline int iMin(int a, int b) { return a < b ? a : b; }
static inline uint16_t u16Max(uint16_t a, uint16_t b) { return a > b ? a : b; }
static inline uint16_t u16Min(uint16_t a, uint16_t b) { return a < b ? a : b; }

static constexpr uint16_t MAX_BLOCKS = 640;
static constexpr uint16_t MAX_RUNS   = 4096;
static constexpr uint16_t MAX_CELLS  = 768;
static constexpr uint16_t MAX_RULES  = 192;
static constexpr uint8_t  MAX_STACK  = 32;
static constexpr uint8_t  MAX_LIST_DEPTH = 8;

enum class BlockKind : uint8_t {
  Paragraph,
  Heading1,
  Heading2,
  Heading3,
  Heading4,
  Heading5,
  Heading6,
  Quote,
  ListItem,
  Pre,
  Image,
  Rule,
  Table
};

enum class Align : uint8_t { Left, Center, Right, Justify };

enum TextFlags : uint8_t {
  TXT_BOLD      = 1 << 0,
  TXT_ITALIC    = 1 << 1,
  TXT_UNDERLINE = 1 << 2
};

struct TextStyle {
  uint16_t fontPx = 26;
  uint8_t flags = 0;
};

struct Run {
  uint32_t textOff = 0;
  uint32_t textLen = 0;
  TextStyle style;
};

struct Cell {
  uint16_t row = 0;
  uint16_t col = 0;
  uint32_t textOff = 0;
  uint32_t textLen = 0;
  bool header = false;
};

struct Block {
  BlockKind kind = BlockKind::Paragraph;
  uint16_t runStart = 0;
  uint16_t runCount = 0;
  uint32_t textLen = 0;       // logical byte length across all runs
  int16_t marginTop = 0;
  int16_t marginBottom = 8;
  int16_t indent = 0;
  int16_t padLeft = 0;
  int16_t padRight = 0;
  int16_t lineSpace = 6;
  Align align = Align::Left;

  // Image block
  uint32_t resourceOff = 0;
  uint16_t resourceLen = 0;

  // Table block
  uint16_t cellStart = 0;
  uint16_t cellCount = 0;
  uint16_t rows = 0;
  uint16_t cols = 0;
};

struct Document {
  char* pool = nullptr;
  size_t poolCap = 0;
  size_t poolLen = 0;
  Block* blocks = nullptr;
  Run* runs = nullptr;
  Cell* cells = nullptr;
  uint16_t blockCount = 0;
  uint16_t runCount = 0;
  uint16_t cellCount = 0;
  int viewportW = 0;
  int viewportH = 0;

  void clear() {
    if (pool) heap_caps_free(pool);
    if (blocks) heap_caps_free(blocks);
    if (runs) heap_caps_free(runs);
    if (cells) heap_caps_free(cells);
    pool = nullptr; blocks = nullptr; runs = nullptr; cells = nullptr;
    poolCap = poolLen = 0;
    blockCount = runCount = cellCount = 0;
    viewportW = viewportH = 0;
  }

  bool alloc(size_t htmlLen) {
    clear();
    // Visible text is normally smaller than XHTML. Keep headroom for list
    // prefixes, decoded entities and resource paths without copying the DOM.
    poolCap = htmlLen + 192U * 1024U;
    if (poolCap < 256U * 1024U) poolCap = 256U * 1024U;
    if (poolCap > 4U * 1024U * 1024U) poolCap = 4U * 1024U * 1024U;
    pool = static_cast<char*>(heap_caps_malloc(poolCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    blocks = static_cast<Block*>(heap_caps_calloc(MAX_BLOCKS, sizeof(Block), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    runs = static_cast<Run*>(heap_caps_calloc(MAX_RUNS, sizeof(Run), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    cells = static_cast<Cell*>(heap_caps_calloc(MAX_CELLS, sizeof(Cell), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!pool || !blocks || !runs || !cells) { clear(); return false; }
    pool[0] = 0;
    return true;
  }

  const char* ptr(uint32_t off) const { return (pool && off < poolLen) ? pool + off : ""; }
};

struct ComputedStyle {
  uint16_t fontPx = 26;
  uint8_t flags = 0;
  Align align = Align::Left;
  int16_t marginTop = 0;
  int16_t marginBottom = 8;
  int16_t indent = 0;
  int16_t padLeft = 0;
  int16_t padRight = 0;
  int16_t lineSpace = 6;
  bool hidden = false;
  bool pre = false;
};

enum PatchBits : uint16_t {
  P_FONT_SIZE = 1 << 0,
  P_BOLD      = 1 << 1,
  P_ITALIC    = 1 << 2,
  P_UNDERLINE = 1 << 3,
  P_ALIGN     = 1 << 4,
  P_MARGIN_T  = 1 << 5,
  P_MARGIN_B  = 1 << 6,
  P_INDENT    = 1 << 7,
  P_PAD_L     = 1 << 8,
  P_PAD_R     = 1 << 9,
  P_HIDDEN    = 1 << 10,
  P_LINE_SPACE = 1 << 11
};

struct StylePatch {
  uint16_t mask = 0;
  ComputedStyle v;
};

struct CssRule {
  char selector[64];
  StylePatch patch;
};

struct ElementCtx {
  char tag[16];
  ComputedStyle style;
  bool block = false;
};

static inline char lowerAscii(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

static bool startsNoCase(const char* data, size_t len, size_t pos, const char* lit) {
  if (!data || !lit) return false;
  for (size_t i = 0; lit[i]; ++i) {
    if (pos + i >= len || lowerAscii(data[pos + i]) != lowerAscii(lit[i])) return false;
  }
  return true;
}

static int findNoCase(const char* data, size_t len, const char* needle, size_t start = 0) {
  if (!data || !needle || !*needle) return -1;
  for (size_t i = start; i < len; ++i) if (startsNoCase(data, len, i, needle)) return static_cast<int>(i);
  return -1;
}

static String sliceTrim(const char* data, size_t a, size_t b) {
  if (!data || b <= a) return String();
  String s;
  s.reserve(b - a + 1);
  for (size_t i = a; i < b; ++i) s += data[i];
  s.trim();
  return s;
}

static String localTagName(String raw) {
  raw.trim();
  while (raw.startsWith("/")) raw.remove(0, 1);
  int sp = raw.indexOf(' ');
  int tab = raw.indexOf('\t');
  int slash = raw.indexOf('/');
  int cut = raw.length();
  if (sp >= 0 && sp < cut) cut = sp;
  if (tab >= 0 && tab < cut) cut = tab;
  if (slash >= 0 && slash < cut) cut = slash;
  raw = raw.substring(0, cut);
  int colon = raw.lastIndexOf(':');
  if (colon >= 0) raw = raw.substring(colon + 1);
  raw.toLowerCase();
  return raw;
}

static bool classHasToken(const String& classes, const String& token) {
  if (!token.length()) return true;
  int p = 0;
  while (p < static_cast<int>(classes.length())) {
    while (p < static_cast<int>(classes.length()) && isspace(static_cast<unsigned char>(classes[p]))) ++p;
    int e = p;
    while (e < static_cast<int>(classes.length()) && !isspace(static_cast<unsigned char>(classes[e]))) ++e;
    if (classes.substring(p, e) == token) return true;
    p = e + 1;
  }
  return false;
}

static int16_t parseLengthPx(String v, uint16_t basePx, int16_t fallback = 0) {
  v.trim(); v.toLowerCase();
  if (!v.length() || v == "auto" || v == "normal") return fallback;
  float n = v.toFloat();
  if (v.endsWith("rem") || v.endsWith("em")) return static_cast<int16_t>(n * basePx);
  if (v.endsWith("%")) return static_cast<int16_t>((n * basePx) / 100.0f);
  if (v.endsWith("pt")) return static_cast<int16_t>(n * 1.3333f);
  return static_cast<int16_t>(n); // px or unitless
}

static Align parseAlign(String v, Align fallback) {
  v.trim(); v.toLowerCase();
  if (v == "center") return Align::Center;
  if (v == "right" || v == "end") return Align::Right;
  if (v == "justify") return Align::Justify;
  if (v == "left" || v == "start") return Align::Left;
  return fallback;
}

static void applyPatch(ComputedStyle& s, const StylePatch& p) {
  if (p.mask & P_FONT_SIZE) s.fontPx = p.v.fontPx;
  if (p.mask & P_BOLD) {
    if (p.v.flags & TXT_BOLD) s.flags |= TXT_BOLD; else s.flags &= ~TXT_BOLD;
  }
  if (p.mask & P_ITALIC) {
    if (p.v.flags & TXT_ITALIC) s.flags |= TXT_ITALIC; else s.flags &= ~TXT_ITALIC;
  }
  if (p.mask & P_UNDERLINE) {
    if (p.v.flags & TXT_UNDERLINE) s.flags |= TXT_UNDERLINE; else s.flags &= ~TXT_UNDERLINE;
  }
  if (p.mask & P_ALIGN) s.align = p.v.align;
  if (p.mask & P_MARGIN_T) s.marginTop = p.v.marginTop;
  if (p.mask & P_MARGIN_B) s.marginBottom = p.v.marginBottom;
  if (p.mask & P_INDENT) s.indent = p.v.indent;
  if (p.mask & P_PAD_L) s.padLeft = p.v.padLeft;
  if (p.mask & P_PAD_R) s.padRight = p.v.padRight;
  if (p.mask & P_LINE_SPACE) s.lineSpace = p.v.lineSpace;
  if (p.mask & P_HIDDEN) s.hidden = p.v.hidden;
}

static void parseDeclaration(const String& keyIn, const String& valueIn, uint16_t basePx, StylePatch& out) {
  String key = keyIn; key.trim(); key.toLowerCase();
  String v = valueIn; v.trim(); v.toLowerCase();
  int imp = v.indexOf("!important"); if (imp >= 0) { v.remove(imp); v.trim(); }

  if (key == "font-weight") {
    out.mask |= P_BOLD;
    bool on = v == "bold" || v == "bolder" || v.toInt() >= 600;
    if (on) out.v.flags |= TXT_BOLD; else out.v.flags &= ~TXT_BOLD;
  } else if (key == "font-style") {
    out.mask |= P_ITALIC;
    bool on = v == "italic" || v == "oblique";
    if (on) out.v.flags |= TXT_ITALIC; else out.v.flags &= ~TXT_ITALIC;
  } else if (key == "text-decoration" || key == "text-decoration-line") {
    out.mask |= P_UNDERLINE;
    if (v.indexOf("underline") >= 0) out.v.flags |= TXT_UNDERLINE; else out.v.flags &= ~TXT_UNDERLINE;
  } else if (key == "font-size") {
    uint16_t px = basePx;
    if (v == "xx-small") px = basePx * 60 / 100;
    else if (v == "x-small") px = basePx * 70 / 100;
    else if (v == "small") px = basePx * 85 / 100;
    else if (v == "medium") px = basePx;
    else if (v == "large") px = basePx * 120 / 100;
    else if (v == "x-large") px = basePx * 145 / 100;
    else if (v == "xx-large") px = basePx * 175 / 100;
    else px = static_cast<uint16_t>(iMax(12, parseLengthPx(v, basePx, basePx)));
    if (px < 14) px = 14; if (px > 58) px = 58;
    out.mask |= P_FONT_SIZE; out.v.fontPx = px;
  } else if (key == "text-align") {
    out.mask |= P_ALIGN; out.v.align = parseAlign(v, Align::Left);
  } else if (key == "text-indent") {
    out.mask |= P_INDENT; out.v.indent = parseLengthPx(v, basePx, 0);
  } else if (key == "margin-top") {
    out.mask |= P_MARGIN_T; out.v.marginTop = parseLengthPx(v, basePx, 0);
  } else if (key == "margin-bottom") {
    out.mask |= P_MARGIN_B; out.v.marginBottom = parseLengthPx(v, basePx, 0);
  } else if (key == "padding-right" || key == "margin-right") {
    out.mask |= P_PAD_R; out.v.padRight = parseLengthPx(v, basePx, 0);
  } else if (key == "padding-left" || key == "margin-left") {
    out.mask |= P_PAD_L; out.v.padLeft = parseLengthPx(v, basePx, 0);
  } else if (key == "line-height") {
    int16_t linePx = parseLengthPx(v, basePx, basePx + 6);
    if (v.endsWith("%")) linePx = static_cast<int16_t>((v.toFloat() * basePx) / 100.0f);
    out.mask |= P_LINE_SPACE; out.v.lineSpace = linePx - basePx;
    if (out.v.lineSpace < -4) out.v.lineSpace = -4;
    if (out.v.lineSpace > 24) out.v.lineSpace = 24;
  } else if (key == "display") {
    out.mask |= P_HIDDEN; out.v.hidden = (v == "none");
  } else if (key == "margin") {
    // Reading-oriented simplification: use first token for vertical margins.
    int sp = v.indexOf(' '); String first = sp >= 0 ? v.substring(0, sp) : v;
    int16_t px = parseLengthPx(first, basePx, 0);
    out.mask |= P_MARGIN_T | P_MARGIN_B; out.v.marginTop = out.v.marginBottom = px;
  }
}

static StylePatch parseDeclarations(String body, uint16_t basePx) {
  StylePatch p;
  int start = 0;
  while (start < static_cast<int>(body.length())) {
    int semi = body.indexOf(';', start); if (semi < 0) semi = body.length();
    int colon = body.indexOf(':', start);
    if (colon >= start && colon < semi) {
      parseDeclaration(body.substring(start, colon), body.substring(colon + 1, semi), basePx, p);
    }
    start = semi + 1;
  }
  return p;
}

static void defaultForTag(const String& tag, uint16_t basePx, ComputedStyle& s) {
  // Inherited properties are already in s. Only reset block box properties.
  s.marginTop = 0; s.marginBottom = iMax(5, basePx / 3); s.indent = 0; s.padLeft = 0; s.padRight = 0;
  if (tag == "strong" || tag == "b") s.flags |= TXT_BOLD;
  else if (tag == "em" || tag == "i" || tag == "cite") s.flags |= TXT_ITALIC;
  else if (tag == "u") s.flags |= TXT_UNDERLINE;
  else if (tag == "small") s.fontPx = u16Max(14, static_cast<uint16_t>(s.fontPx * 85 / 100));
  else if (tag == "big") s.fontPx = u16Min(58, static_cast<uint16_t>(s.fontPx * 120 / 100));
  else if (tag == "h1") { s.fontPx = u16Min(58, static_cast<uint16_t>(basePx * 170 / 100)); s.flags |= TXT_BOLD; s.marginTop = basePx / 2; s.marginBottom = basePx / 2; }
  else if (tag == "h2") { s.fontPx = u16Min(54, static_cast<uint16_t>(basePx * 150 / 100)); s.flags |= TXT_BOLD; s.marginTop = basePx / 2; s.marginBottom = basePx / 2; }
  else if (tag == "h3") { s.fontPx = u16Min(48, static_cast<uint16_t>(basePx * 132 / 100)); s.flags |= TXT_BOLD; s.marginTop = basePx / 2; }
  else if (tag == "h4") { s.fontPx = u16Min(44, static_cast<uint16_t>(basePx * 118 / 100)); s.flags |= TXT_BOLD; }
  else if (tag == "h5") { s.fontPx = u16Min(40, static_cast<uint16_t>(basePx * 108 / 100)); s.flags |= TXT_BOLD; }
  else if (tag == "h6") { s.fontPx = basePx; s.flags |= TXT_BOLD; }
  else if (tag == "blockquote") { s.padLeft += basePx; s.marginTop = basePx / 3; s.marginBottom = basePx / 2; }
  else if (tag == "pre" || tag == "code") { s.pre = true; }
}

static bool selectorMatches(String selector, const String& tag, const String& id, const String& classes) {
  selector.trim(); selector.toLowerCase();
  // Pseudo-elements style only a fragment (first letter, list marker, generated
  // content...). Applying them to the whole element can create gigantic numbers
  // or drop-caps. They need a dedicated renderer, so ignore them for now.
  if (selector.indexOf("::") >= 0 || selector.indexOf(":first-letter") >= 0 ||
      selector.indexOf(":first-line") >= 0) return false;
  // Approximate descendant/child selectors using the last simple selector.
  int sp = selector.lastIndexOf(' '); int gt = selector.lastIndexOf('>');
  int cut = iMax(sp, gt); if (cut >= 0) selector = selector.substring(cut + 1);
  selector.trim();
  int pseudo = selector.indexOf(':'); if (pseudo >= 0) selector.remove(pseudo);
  int attr = selector.indexOf('['); if (attr >= 0) selector.remove(attr);
  if (!selector.length() || selector == "*") return true;

  String needTag, needId;
  String needClasses[3]; int classCount = 0;
  int i = 0;
  while (i < static_cast<int>(selector.length()) && selector[i] != '.' && selector[i] != '#') needTag += selector[i++];
  while (i < static_cast<int>(selector.length())) {
    char prefix = selector[i++];
    String token;
    while (i < static_cast<int>(selector.length()) && selector[i] != '.' && selector[i] != '#') token += selector[i++];
    if (prefix == '#') needId = token;
    else if (prefix == '.' && classCount < 3) needClasses[classCount++] = token;
  }
  needTag.trim();
  if (needTag.length() && needTag != tag) return false;
  if (needId.length() && needId != id) return false;
  for (int c = 0; c < classCount; ++c) if (!classHasToken(classes, needClasses[c])) return false;
  return true;
}

static void parseCssBuffer(const char* data, size_t len, uint16_t basePx, CssRule* rules, uint16_t& ruleCount) {
  if (!data || !len || !rules) return;
  size_t p = 0;
  while (p < len && ruleCount < MAX_RULES) {
    // comments
    if (p + 1 < len && data[p] == '/' && data[p + 1] == '*') {
      int ce = findNoCase(data, len, "*/", p + 2); p = ce < 0 ? len : static_cast<size_t>(ce + 2); continue;
    }
    while (p < len && isspace(static_cast<unsigned char>(data[p]))) ++p;
    if (p >= len) break;
    size_t brace = p; while (brace < len && data[brace] != '{') ++brace;
    if (brace >= len) break;
    String sels = sliceTrim(data, p, brace);
    int depth = 1; size_t e = brace + 1;
    while (e < len && depth) { if (data[e] == '{') ++depth; else if (data[e] == '}') --depth; ++e; }
    if (!depth) {
      if (!sels.startsWith("@")) {
        String body = sliceTrim(data, brace + 1, e - 1);
        StylePatch patch = parseDeclarations(body, basePx);
        int ss = 0;
        while (ss < static_cast<int>(sels.length()) && ruleCount < MAX_RULES) {
          int comma = sels.indexOf(',', ss); if (comma < 0) comma = sels.length();
          String sel = sels.substring(ss, comma); sel.trim();
          if (sel.length() && patch.mask) {
            sel.toCharArray(rules[ruleCount].selector, sizeof(rules[ruleCount].selector));
            rules[ruleCount].patch = patch;
            ++ruleCount;
          }
          ss = comma + 1;
        }
      }
      p = e;
    } else break;
  }
}

static void collectCss(fs::FS& fs, const char* archivePath, const String& chapterPath,
                       const uint8_t* html, size_t len, uint16_t basePx,
                       CssRule* rules, uint16_t& ruleCount) {
  const char* d = reinterpret_cast<const char*>(html);
  size_t p = 0;
  while (p < len && ruleCount < MAX_RULES) {
    int lt = findNoCase(d, len, "<", p); if (lt < 0) break;
    int gt = findNoCase(d, len, ">", lt + 1); if (gt < 0) break;
    String tag = sliceTrim(d, lt + 1, gt);
    String name = localTagName(tag);
    if (name == "style" && !tag.startsWith("/")) {
      int close = findNoCase(d, len, "</style", gt + 1);
      if (close > gt) parseCssBuffer(d + gt + 1, static_cast<size_t>(close - gt - 1), basePx, rules, ruleCount);
      p = close > 0 ? static_cast<size_t>(close + 7) : static_cast<size_t>(gt + 1);
      continue;
    }
    if (name == "link" && !tag.startsWith("/")) {
      String wrapped = "<" + tag + ">";
      String rel = epub_lite::attrValue(wrapped, "rel"); rel.toLowerCase();
      String type = epub_lite::attrValue(wrapped, "type"); type.toLowerCase();
      String href = epub_lite::attrValue(wrapped, "href");
      if (href.length() && (rel.indexOf("stylesheet") >= 0 || type == "text/css")) {
        String cssPath = epub_lite::resolvePackageHref(epub_lite::parentPath(chapterPath), href);
        uint8_t* css = nullptr; size_t cssLen = 0; String err;
        if (epub_lite::extractResource(fs, archivePath, cssPath, &css, &cssLen, 384U * 1024U, err)) {
          parseCssBuffer(reinterpret_cast<const char*>(css), cssLen, basePx, rules, ruleCount);
          heap_caps_free(css);
        }
      }
    }
    p = gt + 1;
  }
}

static bool poolAdd(Document& doc, const char* s, size_t len, uint32_t& offOut) {
  if (!s || !len) { offOut = static_cast<uint32_t>(doc.poolLen); return true; }
  if (!doc.pool || doc.poolLen + len + 1 > doc.poolCap) return false;
  offOut = static_cast<uint32_t>(doc.poolLen);
  memcpy(doc.pool + doc.poolLen, s, len);
  doc.poolLen += len;
  doc.pool[doc.poolLen++] = 0;
  return true;
}

static bool sameTextStyle(const TextStyle& a, const TextStyle& b) {
  return a.fontPx == b.fontPx && a.flags == b.flags;
}

static int newBlock(Document& doc, BlockKind kind, const ComputedStyle& s) {
  if (doc.blockCount >= MAX_BLOCKS) return -1;
  Block& b = doc.blocks[doc.blockCount];
  b = Block();
  b.kind = kind;
  b.runStart = doc.runCount;
  b.marginTop = s.marginTop; b.marginBottom = s.marginBottom;
  b.indent = s.indent; b.padLeft = s.padLeft; b.padRight = s.padRight;
  b.lineSpace = s.lineSpace;
  b.align = s.align;
  return doc.blockCount++;
}

static bool appendRun(Document& doc, int blockIndex, const char* text, size_t len, const ComputedStyle& s) {
  if (blockIndex < 0 || blockIndex >= doc.blockCount || !text || !len || doc.runCount >= MAX_RUNS) return false;
  Block& b = doc.blocks[blockIndex];
  TextStyle ts{ s.fontPx, s.flags };
  uint32_t off = 0;
  if (!poolAdd(doc, text, len, off)) return false;
  Run& r = doc.runs[doc.runCount++];
  r.textOff = off; r.textLen = len; r.style = ts;
  ++b.runCount; b.textLen += len;
  return true;
}

static String decodeText(const uint8_t* data, size_t len, bool pre, bool trimLeading) {
  String out; out.reserve(len + 4);
  bool pendingSpace = false;
  for (size_t i = 0; i < len; ++i) {
    char c = static_cast<char>(data[i]);
    if (c == '&') {
      size_t semi = i + 1;
      while (semi < len && semi - i < 18 && data[semi] != ';') ++semi;
      if (semi < len && data[semi] == ';') {
        char ebuf[20] = {0}; size_t n = semi - i - 1; if (n > 18) n = 18;
        memcpy(ebuf, data + i + 1, n);
        String e(ebuf); uint32_t cp = 0;
        if (e.startsWith("#x") || e.startsWith("#X")) cp = strtoul(e.c_str() + 2, nullptr, 16);
        else if (e.startsWith("#")) cp = strtoul(e.c_str() + 1, nullptr, 10);
        else cp = epub_lite::namedEntity(e);
        if (cp) {
          char u[5] = {0}; int un = 0;
          if (cp <= 0x7F) u[un++] = static_cast<char>(cp);
          else if (cp <= 0x7FF) { u[un++] = 0xC0 | (cp >> 6); u[un++] = 0x80 | (cp & 0x3F); }
          else if (cp <= 0xFFFF) { u[un++] = 0xE0 | (cp >> 12); u[un++] = 0x80 | ((cp >> 6)&0x3F); u[un++] = 0x80 | (cp&0x3F); }
          else { u[un++] = 0xF0 | (cp >> 18); u[un++] = 0x80 | ((cp >> 12)&0x3F); u[un++] = 0x80 | ((cp >> 6)&0x3F); u[un++] = 0x80 | (cp&0x3F); }
          if (!pre && cp == ' ') pendingSpace = true;
          else { if (pendingSpace && out.length()) out += ' '; pendingSpace = false; for (int k=0;k<un;++k) out += u[k]; }
          i = semi; continue;
        }
      }
    }
    if (!pre && (c == ' ' || c == '\r' || c == '\n' || c == '\t')) { pendingSpace = true; continue; }
    if (pendingSpace && (!trimLeading || out.length())) out += ' ';
    pendingSpace = false;
    out += c;
  }
  if (!pre) while (out.endsWith(" ")) out.remove(out.length() - 1);
  return out;
}

static BlockKind blockKindForTag(const String& tag) {
  if (tag == "h1") return BlockKind::Heading1;
  if (tag == "h2") return BlockKind::Heading2;
  if (tag == "h3") return BlockKind::Heading3;
  if (tag == "h4") return BlockKind::Heading4;
  if (tag == "h5") return BlockKind::Heading5;
  if (tag == "h6") return BlockKind::Heading6;
  if (tag == "blockquote") return BlockKind::Quote;
  if (tag == "li") return BlockKind::ListItem;
  if (tag == "pre") return BlockKind::Pre;
  return BlockKind::Paragraph;
}

static bool isBlockTag(const String& tag) {
  return tag == "p" || tag == "div" || tag == "section" || tag == "article" || tag == "aside" ||
         tag == "header" || tag == "footer" || tag == "h1" || tag == "h2" || tag == "h3" ||
         tag == "h4" || tag == "h5" || tag == "h6" || tag == "blockquote" || tag == "li" || tag == "pre";
}

static void applyRules(ComputedStyle& s, const String& tag, const String& id, const String& classes,
                       const CssRule* rules, uint16_t ruleCount) {
  for (uint16_t i = 0; i < ruleCount; ++i) {
    if (selectorMatches(String(rules[i].selector), tag, id, classes)) applyPatch(s, rules[i].patch);
  }
}

static bool appendCell(Document& doc, int tableBlock, uint16_t row, uint16_t col, const String& txt, bool header) {
  if (tableBlock < 0 || tableBlock >= doc.blockCount || doc.cellCount >= MAX_CELLS) return false;
  uint32_t off = 0;
  if (!poolAdd(doc, txt.c_str(), txt.length(), off)) return false;
  Cell& c = doc.cells[doc.cellCount++];
  c.row = row; c.col = col; c.textOff = off; c.textLen = txt.length(); c.header = header;
  Block& b = doc.blocks[tableBlock];
  if (!b.cellCount) b.cellStart = doc.cellCount - 1;
  ++b.cellCount;
  b.rows = u16Max(b.rows, static_cast<uint16_t>(row + 1));
  b.cols = u16Max(b.cols, static_cast<uint16_t>(col + 1));
  return true;
}

static bool parseHtml(Document& doc, fs::FS& fs, const char* archivePath, const String& chapterPath,
                      const uint8_t* html, size_t len, uint16_t basePx, String& err) {
  CssRule* rules = static_cast<CssRule*>(heap_caps_calloc(MAX_RULES, sizeof(CssRule), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!rules) { err = "Mémoire CSS EPUB insuffisante"; return false; }
  uint16_t ruleCount = 0;
  collectCss(fs, archivePath, chapterPath, html, len, basePx, rules, ruleCount);

  ElementCtx stack[MAX_STACK]; int depth = 0;
  ComputedStyle root; root.fontPx = basePx; root.marginBottom = iMax(5, basePx / 3);
  stack[0].style = root; strcpy(stack[0].tag, "root"); depth = 1;
  int currentBlock = -1;
  bool inBody = false;
  int skipDepth = 0;
  bool listOrdered[MAX_LIST_DEPTH] = {false};
  uint16_t listCounter[MAX_LIST_DEPTH] = {0};
  uint8_t listDepth = 0;

  int tableBlock = -1; int tableRow = -1; int tableCol = 0; bool inCell = false; bool cellHeader = false; String cellText;

  size_t textStart = 0;
  auto flushText = [&](size_t a, size_t b) {
    if (b <= a || skipDepth > 0 || !inBody) return;
    const ComputedStyle& st = stack[depth - 1].style;
    if (st.hidden) return;
    String txt = decodeText(html + a, b - a, st.pre, currentBlock < 0);
    if (!txt.length()) return;
    if (tableBlock >= 0 && inCell) {
      if (cellText.length() && !cellText.endsWith(" ")) cellText += ' ';
      cellText += txt; return;
    }
    if (currentBlock < 0) currentBlock = newBlock(doc, BlockKind::Paragraph, st);
    if (currentBlock >= 0) appendRun(doc, currentBlock, txt.c_str(), txt.length(), st);
  };

  for (size_t i = 0; i < len; ) {
    if ((i & 0x0FFFu) == 0) yield(); // feed RTOS/watchdog on large XHTML sections
    if (html[i] != '<') { ++i; continue; }
    flushText(textStart, i);
    size_t gt = i + 1; bool quoted = false; char qch = 0;
    while (gt < len) {
      char c = static_cast<char>(html[gt]);
      if (quoted) { if (c == qch) quoted = false; }
      else if (c == '\'' || c == '"') { quoted = true; qch = c; }
      else if (c == '>') break;
      ++gt;
    }
    if (gt >= len) break;
    String raw = sliceTrim(reinterpret_cast<const char*>(html), i + 1, gt);
    bool closing = raw.startsWith("/");
    bool selfClose = raw.endsWith("/");
    String tag = localTagName(raw);

    if (tag == "body" && !closing) inBody = true;
    if (tag == "head") { if (!closing) ++skipDepth; else if (skipDepth) --skipDepth; }
    if (tag == "script" || tag == "style" || tag == "title") { if (!closing) ++skipDepth; else if (skipDepth) --skipDepth; }

    if (inBody && skipDepth == 0) {
      if (!closing) {
        String wrapped = "<" + raw + ">";
        String id = epub_lite::attrValue(wrapped, "id"); id.toLowerCase();
        String classes = epub_lite::attrValue(wrapped, "class"); classes.toLowerCase();
        ComputedStyle st = stack[depth - 1].style;
        defaultForTag(tag, basePx, st);
        applyRules(st, tag, id, classes, rules, ruleCount);
        String inlineStyle = epub_lite::attrValue(wrapped, "style");
        if (inlineStyle.length()) applyPatch(st, parseDeclarations(inlineStyle, st.fontPx));
        if (tag == "sup" || tag == "sub") st.fontPx = u16Max(14, static_cast<uint16_t>(st.fontPx * 75 / 100));

        if (tag == "ul" || tag == "ol") {
          if (listDepth < MAX_LIST_DEPTH) { listOrdered[listDepth] = (tag == "ol"); listCounter[listDepth] = 0; ++listDepth; }
          currentBlock = -1;
        } else if (tag == "table") {
          currentBlock = -1; tableBlock = newBlock(doc, BlockKind::Table, st); tableRow = -1; tableCol = 0;
        } else if (tag == "tr" && tableBlock >= 0) {
          ++tableRow; tableCol = 0; inCell = false;
        } else if ((tag == "td" || tag == "th") && tableBlock >= 0) {
          inCell = true; cellHeader = (tag == "th"); cellText = "";
        } else if (tag == "img" || tag == "image") {
          currentBlock = -1;
          String src = epub_lite::attrValue(wrapped, "src"); if (!src.length()) src = epub_lite::attrValue(wrapped, "href");
          if (!src.length()) src = epub_lite::attrValue(wrapped, "xlink:href");
          if (src.length() && !src.startsWith("data:")) {
            String full = epub_lite::resolvePackageHref(epub_lite::parentPath(chapterPath), src);
            int bi = newBlock(doc, BlockKind::Image, st);
            if (bi >= 0) {
              uint32_t off = 0; if (poolAdd(doc, full.c_str(), full.length(), off)) { doc.blocks[bi].resourceOff = off; doc.blocks[bi].resourceLen = full.length(); }
            }
          }
        } else if (tag == "hr") {
          currentBlock = -1; newBlock(doc, BlockKind::Rule, st);
        } else if (tag == "br") {
          if (tableBlock >= 0 && inCell) cellText += '\n';
          else if (currentBlock >= 0) appendRun(doc, currentBlock, "\n", 1, st);
        } else if (isBlockTag(tag)) {
          currentBlock = newBlock(doc, blockKindForTag(tag), st);
          if (currentBlock >= 0 && tag == "li") {
            doc.blocks[currentBlock].padLeft += static_cast<int16_t>(iMax(0, listDepth) * (basePx * 3 / 4));
            String prefix;
            if (listDepth && listOrdered[listDepth - 1]) prefix = String(++listCounter[listDepth - 1]) + ". ";
            else prefix = "• ";
            // Marker CSS (::marker) is not rendered yet. Keep the marker close
            // to the normal body size so a publisher's special marker/drop-cap
            // rule cannot make ordered-list numbers dwarf the list text.
            ComputedStyle markerStyle = st;
            if (markerStyle.fontPx > basePx) markerStyle.fontPx = basePx;
            appendRun(doc, currentBlock, prefix.c_str(), prefix.length(), markerStyle);
          }
        }

        if (!selfClose && depth < MAX_STACK) {
          raw.toLowerCase();
          tag.toCharArray(stack[depth].tag, sizeof(stack[depth].tag));
          stack[depth].style = st; stack[depth].block = isBlockTag(tag);
          ++depth;
        }
      } else {
        if ((tag == "td" || tag == "th") && tableBlock >= 0 && inCell) {
          cellText.trim(); appendCell(doc, tableBlock, iMax(0, tableRow), tableCol++, cellText, cellHeader); inCell = false; cellText = "";
        }
        if (tag == "table") { tableBlock = -1; tableRow = -1; currentBlock = -1; }
        if (tag == "ul" || tag == "ol") { if (listDepth) --listDepth; currentBlock = -1; }
        if (isBlockTag(tag)) currentBlock = -1;
        if (tag == "body") inBody = false;

        // Pop to matching tag. EPUB XHTML is XML, but this tolerance helps malformed books.
        for (int d = depth - 1; d > 0; --d) {
          if (tag == stack[d].tag) { depth = d; break; }
        }
      }
    }
    i = gt + 1; textStart = i;
  }
  flushText(textStart, len);
  heap_caps_free(rules);

  // Remove empty textual blocks generated by structural wrappers.
  uint16_t out = 0;
  for (uint16_t i = 0; i < doc.blockCount; ++i) {
    Block& b = doc.blocks[i];
    bool keep = b.kind == BlockKind::Image || b.kind == BlockKind::Rule || b.kind == BlockKind::Table || b.textLen > 0;
    if (keep) { if (out != i) doc.blocks[out] = b; ++out; }
  }
  doc.blockCount = out;
  return doc.blockCount > 0;
}

static bool buildDocument(fs::FS& fs, const char* archivePath, const epub_lite::Book& book,
                          uint16_t chapterIndex, uint16_t baseFontPx, Document& doc, String& err) {
  doc.clear();
  if (!book.chapters || chapterIndex >= book.chapterCount) { err = "Section EPUB invalide"; return false; }
  const String chapterPath(book.chapters[chapterIndex]);
  uint8_t* html = nullptr; size_t htmlLen = 0;
  if (!epub_lite::extractResource(fs, archivePath, chapterPath, &html, &htmlLen,
                                  epub_lite::MAX_CHAPTER_HTML_BYTES, err)) return false;
  if (!doc.alloc(htmlLen)) { heap_caps_free(html); err = "Mémoire mise en page EPUB insuffisante"; return false; }
  epub_lite::parseViewportFromHtml(html, htmlLen, doc.viewportW, doc.viewportH);
  bool ok = parseHtml(doc, fs, archivePath, chapterPath, html, htmlLen, baseFontPx, err);
  heap_caps_free(html);
  if (!ok) { if (!err.length()) err = "Contenu XHTML EPUB vide"; doc.clear(); }
  return ok;
}

static inline bool isTextBlock(BlockKind k) {
  return k != BlockKind::Image && k != BlockKind::Rule && k != BlockKind::Table;
}

} // namespace epub_render
