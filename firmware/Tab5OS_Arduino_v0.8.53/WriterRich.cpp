#include "WriterRich.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace writing {
namespace {
uint32_t decode(const std::string& s, size_t& p) {
  unsigned char c = s[p++];
  if (c < 128) return c;
  int n = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : 3;
  uint32_t out = c & ((1u << (6 - n)) - 1);
  while (n-- && p < s.size()) out = (out << 6) | (s[p++] & 63);
  return out;
}
std::string escape(uint32_t c) {
  if (c == '<') return "&lt;";
  if (c == '>') return "&gt;";
  if (c == '&') return "&amp;";
  std::string s = RichText::utf8(c);
  if (c == '*' || c == '_' || c == '`' || c == '[' || c == ']' || c == '\\') s = "\\" + s;
  return s;
}
void inlineParse(const std::string& text, size_t begin, size_t end, uint8_t style, uint16_t link,
                 RichLine& line, ExternalVector<RichLink>& links, unsigned depth = 0,
                 uint32_t fg = NoColor, uint32_t bg = NoColor) {
  if (depth > 12) {
    for (size_t p = begin; p < end;) {
      size_t at = p;
      line.glyphs.push_back({decode(text, p), style, link, at, fg, bg});
    }
    return;
  }
  for (size_t i = begin; i < end;) {
    size_t source = i;
    if (text.compare(i, 13, "<span style=\"") == 0) {
      size_t tagEnd = text.find("\">", i + 13), close = text.find("</span>", tagEnd);
      if (tagEnd < end && close < end) {
        std::string css = text.substr(i + 13, tagEnd - i - 13);
        uint32_t fore = fg, back = bg;
        bool valid = true, found = false;
        for (size_t p = 0; p < css.size();) {
          size_t stop = css.find(';', p);
          if (stop == std::string::npos) stop = css.size();
          std::string item = css.substr(p, stop - p);
          p = stop + 1;
          item.erase(std::remove_if(item.begin(), item.end(),
                                    [](unsigned char c) { return std::isspace(c); }),
                     item.end());
          if (item.empty()) continue;
          size_t colon = item.find(':');
          std::string name = item.substr(0, colon);
          if (colon == std::string::npos || item.size() != colon + 8 || item[colon + 1] != '#' ||
              (name != "color" && name != "background-color")) {
            valid = false;
            break;
          }
          uint32_t value = 0;
          for (size_t j = colon + 2; j < item.size(); ++j) {
            char c = item[j];
            int h = c >= '0' && c <= '9'   ? c - '0'
                    : c >= 'a' && c <= 'f' ? c - 'a' + 10
                    : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                           : -1;
            if (h < 0) {
              valid = false;
              break;
            }
            value = (value << 4) | h;
          }
          if (name == "color")
            fore = value;
          else
            back = value;
          found = true;
        }
        if (valid && found) {
          inlineParse(text, tagEnd + 2, close, style, link, line, links, depth + 1, fore, back);
          i = close + 7;
          continue;
        }
      }
    }
    const char* entities[] = {"&amp;", "&lt;", "&gt;", "&quot;", "&#39;", "&#124;"};
    const uint32_t entityCodes[] = {'&', '<', '>', '\"', '\'', '|'};
    bool entity = false;
    for (unsigned n = 0; n < 6; ++n) {
      size_t len = std::char_traits<char>::length(entities[n]);
      if (i + len <= end && text.compare(i, len, entities[n]) == 0) {
        line.glyphs.push_back({entityCodes[n], style, link, i, fg, bg});
        i += len;
        entity = true;
        break;
      }
    }
    if (entity) continue;
    if (text[i] == '\\' && i + 1 < end && std::ispunct(static_cast<unsigned char>(text[i + 1]))) {
      ++i;
      line.glyphs.push_back({decode(text, i), style, link, source, fg, bg});
      continue;
    }
    if (!link && text.compare(i, 2, "[[") == 0) {
      size_t close = text.find("]]", i + 2);
      if (close < end) {
        size_t pipe = text.find('|', i + 2);
        if (pipe >= close) pipe = close;
        links.push_back({text.substr(i + 2, pipe - i - 2), true});
        inlineParse(text, pipe < close ? pipe + 1 : i + 2, close, style, links.size(), line, links,
                    depth + 1, fg, bg);
        i = close + 2;
        continue;
      }
    }
    if (!link && text[i] == '[') {
      size_t mid = text.find("](", i + 1),
             close = mid == std::string::npos ? mid : text.find(')', mid + 2);
      if (close < end) {
        links.push_back({text.substr(mid + 2, close - mid - 2), false});
        inlineParse(text, i + 1, mid, style, links.size(), line, links, depth + 1, fg, bg);
        i = close + 1;
        continue;
      }
    }
    if(text.compare(i,3,"<u>")==0){size_t close=text.find("</u>",i+3);if(close<end){inlineParse(text,i+3,close,style|8,link,line,links,depth+1,fg,bg);i=close+4;continue;}}
    char c = text[i];
    if (c == '*' || c == '_' || c == '`' || c == '~') {
      size_t n = 1;
      while (i + n < end && text[i + n] == c) ++n;
      if (n > 3 || (c == '`' && n > 1) || (c == '~' && n != 2)) n = 0;
      const bool intraword =
          c == '_' && i > begin && std::isalnum(static_cast<unsigned char>(text[i - 1]));
      if (n && !intraword && i + n < end &&
          !std::isspace(static_cast<unsigned char>(text[i + n]))) {
        std::string delim(n, c);
        size_t close = text.find(delim, i + n);
        while (close < end &&
               (close == i + n || std::isspace(static_cast<unsigned char>(text[close - 1])) ||
                (close && text[close - 1] == '\\')))
          close = text.find(delim, close + n);
        if (close < end) {
          uint8_t added = c == '~' ? 16 : c == '`' ? 4 : n == 3 ? 3 : n == 2 ? 1 : 2;
          if (c == '`') {
            for (size_t p = i + n; p < close;) {
              size_t at = p;
              line.glyphs.push_back(
                  {decode(text, p), static_cast<uint8_t>(style | 4), link, at, fg, bg});
            }
          } else
            inlineParse(text, i + n, close, style | added, link, line, links, depth + 1, fg, bg);
          i = close + n;
          continue;
        }
      }
    }
    line.glyphs.push_back({decode(text, i), style, link, source, fg, bg});
  }
}
std::string trim(std::string s) {
  size_t a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}
std::vector<std::string> tableRow(const std::string& source) {
  std::string row = trim(source);
  std::vector<std::string> cells;
  if (row.find('|') == std::string::npos) return cells;
  size_t a = !row.empty() && row[0] == '|' ? 1 : 0;
  bool escaped = false;
  unsigned ticks = 0;
  for (size_t i = a; i <= row.size(); ++i) {
    if (i < row.size() && escaped) {
      escaped = false;
      continue;
    }
    if (i < row.size() && row[i] == '\\') {
      escaped = true;
      continue;
    }
    if (i < row.size() && row[i] == '`') {
      ticks ^= 1;
      continue;
    }
    if (i == row.size() || (row[i] == '|' && !ticks)) {
      if (i != row.size() || i > a) cells.push_back(trim(row.substr(a, i - a)));
      a = i + 1;
    }
  }
  return cells;
}
bool tableSeparator(const std::vector<std::string>& cells, std::vector<uint8_t>& align) {
  if (cells.empty()) return false;
  for (auto& s : cells) {
    size_t a = 0, b = s.size();
    uint8_t mode = 0;
    if (a < b && s[a] == ':') {
      ++a;
      mode |= 1;
    }
    if (a < b && s[b - 1] == ':') {
      --b;
      mode |= 2;
    }
    if (a == b) return false;
    for (size_t i = a; i < b; ++i)
      if (s[i] != '-') return false;
    align.push_back(mode);
  }
  return true;
}
}  // namespace
std::string RichText::utf8(uint32_t c) {
  std::string r;
  if (c < 128)
    r += char(c);
  else if (c < 2048) {
    r += char(0xc0 | (c >> 6));
    r += char(0x80 | (c & 63));
  } else if (c < 65536) {
    r += char(0xe0 | (c >> 12));
    r += char(0x80 | ((c >> 6) & 63));
    r += char(0x80 | (c & 63));
  } else {
    r += char(0xf0 | (c >> 18));
    r += char(0x80 | ((c >> 12) & 63));
    r += char(0x80 | ((c >> 6) & 63));
    r += char(0x80 | (c & 63));
  }
  return r;
}
void RichText::parse(const std::string& source, bool fenced, bool raw) {
  lines.clear();
  links.clear();
  size_t p = 0;
  do {
    if (!fenced && !raw) {
      size_t firstEnd = source.find('\n', p);
      if (firstEnd == std::string::npos) firstEnd = source.size();
      std::string first = source.substr(p, firstEnd - p), clean = trim(first);
      RichLine block;
      block.source = p;
      size_t finish = p;
      if (clean.rfind("![", 0) == 0 && !clean.empty() && clean.back() == ')') {
        size_t mid = clean.find("](", 2);
        if (mid != std::string::npos) {
          block.block = 2;
          block.imageAlt = clean.substr(2, mid - 2);
          block.imagePath = clean.substr(mid + 2, clean.size() - mid - 3);
          if (block.imagePath.size() > 1 && block.imagePath.front() == '<' &&
              block.imagePath.back() == '>')
            block.imagePath = block.imagePath.substr(1, block.imagePath.size() - 2);
          finish = firstEnd + (firstEnd < source.size() ? 1 : 0);
        }
      }
      if (!block.block && firstEnd < source.size()) {
        auto header = tableRow(first);
        size_t secondEnd = source.find('\n', firstEnd + 1);
        if (secondEnd == std::string::npos) secondEnd = source.size();
        auto sep = tableRow(source.substr(firstEnd + 1, secondEnd - firstEnd - 1));
        if (!header.empty() && header.size() <= 12 && sep.size() == header.size() &&
            tableSeparator(sep, block.alignment)) {
          block.block = 1;
          block.cells.push_back(header);
          finish = secondEnd + (secondEnd < source.size() ? 1 : 0);
          while (finish < source.size() && block.cells.size() < 128) {
            size_t next = source.find('\n', finish);
            if (next == std::string::npos) next = source.size();
            auto row = tableRow(source.substr(finish, next - finish));
            if (row.empty()) break;
            row.resize(header.size());
            block.cells.push_back(std::move(row));
            finish = next + (next < source.size() ? 1 : 0);
          }
        }
      }
      if (block.block) {
        block.original.assign(source.data() + p, finish - p);
        if (finish > p && source[finish - 1] == '\n')
          block.ending = finish > p + 1 && source[finish - 2] == '\r' ? "\r\n" : "\n";
        block.glyphs.push_back({0xfffc, 0, 0, p});
        lines.push_back(std::move(block));
        p = finish;
        if (finish == source.size() && (finish == 0 || source[finish - 1] != '\n')) break;
        continue;
      }
    }
    size_t e = source.find('\n', p);
    bool nl = e != std::string::npos;
    if (!nl) e = source.size();
    RichLine line;
    line.source = p;
    line.original.assign(source.data() + p, e - p + (nl ? 1 : 0));
    size_t contentEnd = e;
    if (nl) {
      line.ending = "\n";
      if (contentEnd > p && source[contentEnd - 1] == '\r') {
        --contentEnd;
        line.ending = "\r\n";
      }
    }
    size_t a = p, hash = 0;
    while (a + hash < contentEnd && source[a + hash] == '#' && hash < 6) ++hash;
    bool fence = source.compare(p, 3, "```") == 0 || source.compare(p, 3, "~~~") == 0;
    line.literal = raw || fenced || fence;
    if (fence && !raw) fenced = !fenced;
    if (!line.literal) {
      if (hash && a + hash < contentEnd && source[a + hash] == ' ') {
        line.heading = hash;
        a += hash + 1;
      } else if (source.compare(a, 2, "> ") == 0 || source.compare(a, 2, "- ") == 0 ||
                 source.compare(a, 2, "+ ") == 0 || source.compare(a, 2, "* ") == 0)
        a += 2;
      else {
        size_t d = a;
        while (d < contentEnd && std::isdigit(static_cast<unsigned char>(source[d]))) ++d;
        if (d > a && d + 1 < contentEnd && (source[d] == '.' || source[d] == ')') &&
            source[d + 1] == ' ')
          a = d + 2;
      }
    }
    line.prefix = source.substr(p, a - p);
    if (line.literal) {
      while (a < contentEnd) {
        size_t at = a;
        line.glyphs.push_back({decode(source, a), 4, 0, at});
      }
    } else
      inlineParse(source, a, contentEnd, 0, 0, line, links);
    lines.push_back(std::move(line));
    p = e + (nl ? 1 : 0);
    if (!nl) break;
  } while (p <= source.size());
}
size_t RichText::size() const {
  size_t n = 0;
  for (auto& l : lines) n += l.glyphs.size() + (l.ending.empty() ? 0 : 1);
  return n;
}
void RichText::locate(size_t i, size_t& l, size_t& c) const {
  for (l = 0; l < lines.size(); ++l) {
    if (i <= lines[l].glyphs.size()) {
      c = i;
      return;
    }
    i -= lines[l].glyphs.size() + (lines[l].ending.empty() ? 0 : 1);
  }
  l = lines.empty() ? 0 : lines.size() - 1;
  c = lines.empty() ? 0 : lines[l].glyphs.size();
}
RichGlyph RichText::glyph(size_t i) const {
  size_t l, c;
  locate(i, l, c);
  if (lines.empty()) return {};
  if (c < lines[l].glyphs.size()) return lines[l].glyphs[c];
  return {'\n', 0, 0, lines[l].source + lines[l].original.size() - lines[l].ending.size()};
}
size_t RichText::sourceAt(size_t i) const {
  size_t l, c;
  locate(i, l, c);
  if (lines.empty()) return 0;
  if (c < lines[l].glyphs.size()) return lines[l].glyphs[c].source;
  return lines[l].source + lines[l].original.size() - lines[l].ending.size();
}
size_t RichText::indexAtSource(size_t src) const {
  size_t i = 0;
  for (auto& l : lines) {
    for (auto& g : l.glyphs) {
      if (g.source >= src) return i;
      ++i;
    }
    if (!l.ending.empty()) {
      if (l.source + l.original.size() - l.ending.size() >= src) return i;
      ++i;
    }
  }
  return i;
}
std::string RichText::fragment(size_t a, size_t b) const {
  RichText part;
  part.links = links;
  size_t position = 0;
  a = std::min(a, size());
  b = std::max(a, std::min(b, size()));
  for (auto& line : lines) {
    size_t count = line.glyphs.size() + (line.ending.empty() ? 0 : 1);
    size_t first = std::max(a, position), last = std::min(b, position + count);
    if (first < last) {
      size_t ca = first - position, cb = last - position;
      RichLine piece = line;
      if (ca != 0 || cb != count) {
        if (line.block) {
          if (ca == 0) {
            piece.original = line.original.substr(0, line.original.size() - line.ending.size());
            piece.ending = "";
          } else {
            piece = RichLine{};
            piece.ending = line.ending;
            piece.original.assign(line.ending.data(), line.ending.size());
          }
        } else {
          piece.glyphs =
              ExternalVector<RichGlyph>(line.glyphs.begin() + std::min(ca, line.glyphs.size()),
                                     line.glyphs.begin() + std::min(cb, line.glyphs.size()));
          piece.ending = cb > line.glyphs.size() ? line.ending : "";
          piece.dirty = true;
          if (ca) {
            piece.prefix = "";
            piece.heading = 0;
          }
        }
      }
      part.lines.push_back(std::move(piece));
    }
    position += count;
    if (position >= b) break;
  }
  return part.markdown();
}
std::string RichText::plain(size_t a, size_t b) const {
  std::string s;
  size_t position = 0;
  for (const auto& line : lines) {
    const size_t count = line.glyphs.size();
    if (position >= b) break;
    if (a < position + count && b > position) {
      const size_t first = a > position ? a - position : 0;
      const size_t last = std::min(count, b - position);
      for (size_t i = first; i < last; ++i) s += utf8(line.glyphs[i].code);
    }
    position += count;
    if (!line.ending.empty()) {
      if (position >= a && position < b) s += '\n';
      ++position;
    }
  }
  return s;
}
uint8_t RichText::styleAt(size_t i) const {
  if (!size()) return 0;
  return glyph(i ? i - 1 : 0).style & 27;
}
bool RichText::allStyle(size_t a, size_t b, uint8_t bit) const {
  if (a == b) return false;
  size_t position = 0;
  for (const auto& line : lines) {
    const size_t count = line.glyphs.size();
    if (position >= b) break;
    if (a < position + count && b > position) {
      const size_t first = a > position ? a - position : 0;
      const size_t last = std::min(count, b - position);
      for (size_t i = first; i < last; ++i)
        if (line.glyphs[i].code != '\n' && !(line.glyphs[i].style & bit)) return false;
    }
    position += count + (line.ending.empty() ? 0 : 1);
  }
  return true;
}
void RichText::toggle(size_t a, size_t b, uint8_t bit) {
  bool remove = allStyle(a, b, bit);
  size_t i = 0;
  for (auto& l : lines) {
    for (auto& g : l.glyphs) {
      if (i >= a && i < b && !l.literal && !l.block) {
        g.style = remove ? g.style & ~bit : g.style | bit;
        l.dirty = true;
      }
      ++i;
    }
    if (!l.ending.empty()) ++i;
  }
}
void RichText::color(size_t a, size_t b, uint32_t value, bool background) {
  size_t i = 0;
  for (auto& l : lines) {
    for (auto& g : l.glyphs) {
      if (i >= a && i < b && !l.literal && !l.block) {
        (background ? g.background : g.foreground) = value;
        l.dirty = true;
      }
      ++i;
    }
    if (!l.ending.empty()) ++i;
  }
}
size_t RichText::replace(size_t a, size_t b, const std::string& text, uint8_t style,
                         uint32_t foreground, uint32_t background) {
  if (lines.empty()) parse("");
  a = std::min(a, size());
  b = std::max(a, std::min(b, size()));
  size_t la, ca, lb, cb;
  locate(a, la, ca);
  locate(b, lb, cb);
  bool block = false;
  for (size_t i = la; i <= lb; ++i) block |= lines[i].block != 0;
  if (block) {
    std::string left = fragment(0, a), right = fragment(b, size());
    RichText inserted;
    inserted.parse("");
    inserted.replace(0, 0, text, style, foreground, background);
    std::string value = inserted.markdown();
    bool leftBlock = lines[la].block && ca > 0;
    bool rightBlock = lines[lb].block && cb == 0;
    if (leftBlock && (!value.empty() || !right.empty()) && !left.empty() && left.back() != '\n')
      left += '\n';
    std::string combined = left + value;
    if (rightBlock && !combined.empty() && combined.back() != '\n') combined += '\n';
    size_t after = combined.size();
    combined += right;
    parse(combined);
    return indexAtSource(after);
  }
  RichLine first = lines[la];
  ExternalVector<RichGlyph> tail(lines[lb].glyphs.begin() + cb, lines[lb].glyphs.end());
  std::string ending = lines[lb].ending;
  first.glyphs.resize(ca);
  first.dirty = true;
  first.ending = "";
  ExternalVector<RichLine> added;
  added.push_back(std::move(first));
  size_t count = 0;
  for (size_t p = 0; p < text.size();) {
    uint32_t c = decode(text, p);
    if (c == '\r' && p < text.size() && text[p] == '\n') continue;
    ++count;
    if (c == '\n') {
      added.back().ending = ending == "\r\n" ? "\r\n" : "\n";
      RichLine next;
      next.literal = added.back().literal;
      next.dirty = true;
      added.push_back(std::move(next));
    } else
      added.back().glyphs.push_back({c, static_cast<uint8_t>(added.back().literal ? 4 : style), 0,
                                     0, foreground, background});
  }
  added.back().glyphs.insert(added.back().glyphs.end(), tail.begin(), tail.end());
  added.back().ending = ending;
  lines.erase(lines.begin() + la, lines.begin() + lb + 1);
  lines.insert(lines.begin() + la, added.begin(), added.end());
  return a + count;
}
const RichLine* RichText::blockAt(size_t index) const {
  size_t l, c;
  locate(index, l, c);
  return !lines.empty() && lines[l].block && c < lines[l].glyphs.size() ? &lines[l] : nullptr;
}
std::string tableMarkdown(const std::vector<std::vector<std::string>>& cells,
                          const std::vector<uint8_t>& alignment) {
  if (cells.empty() || cells[0].empty()) return "";
  std::string out;
  for (size_t r = 0; r < cells.size(); ++r) {
    out += '|';
    for (auto& cell : cells[r]) {
      out += ' ';
      out += cell;
      out += " |";
    }
    out += '\n';
    if (r == 0) {
      out += '|';
      for (size_t c = 0; c < cells[0].size(); ++c) {
        uint8_t a = c < alignment.size() ? alignment[c] : 0;
        out += ' ';
        if (a & 1) out += ':';
        out += "---";
        if (a & 2) out += ':';
        out += " |";
      }
      out += '\n';
    }
  }
  return out;
}
void RichText::parseCell(const std::string& source) {
  // GFM cells have inline content. HTML h1..h6 is portable inside a cell;
  // ATX '# ' would be displayed literally by other Markdown readers.
  if (source.size() >= 9 && source[0] == '<' && source[1] == 'h' && source[2] >= '1' &&
      source[2] <= '6' && source[3] == '>' &&
      source.substr(source.size() - 5) == std::string("</h") + source[2] + ">") {
    parse(std::string(source[2] - '0', '#') + " " + source.substr(4, source.size() - 9));
  } else
    parse(source);
}
std::string RichText::cellMarkdown() const {
  std::string value;
  for (const auto& line : lines) {
    if (!value.empty()) value += " ";
    if (line.heading) value += "<h" + std::to_string(line.heading) + ">";
    value += inlineMarkdown(line);
    if (line.heading) value += "</h" + std::to_string(line.heading) + ">";
  }
  std::string out;
  for (char c : value) {
    if (c == '|')
      out += "&#124;";
    else
      out += (c == '\n' || c == '\r') ? ' ' : c;
  }
  return out;
}
void RichText::heading(size_t index) {
  size_t l, c;
  locate(index, l, c);
  if (lines.empty() || lines[l].literal || lines[l].block) return;
  auto& line = lines[l];
  line.heading = (line.heading + 1) % 4;
  line.prefix = line.heading ? std::string(line.heading, '#') + " " : "";
  line.dirty = true;
}
std::string RichText::inlineMarkdown(const RichLine& line, std::vector<size_t>* offsets) const {
  if (line.literal) {
    std::string plain;
    for (auto& g : line.glyphs) {
      if (offsets) offsets->push_back(plain.size());
      plain += utf8(g.code);
    }
    return plain;
  }
  std::string out;
  uint32_t fore = NoColor, back = NoColor;
  auto closeColor = [&]() {
    if (fore != NoColor || back != NoColor) out += "</span>";
    fore = back = NoColor;
  };
  auto openColor = [&](uint32_t f, uint32_t b) {
    fore = f;
    back = b;
    if (f == NoColor && b == NoColor) return;
    out += "<span style=\"";
    char code[8];
    if (f != NoColor) {
      snprintf(code, sizeof(code), "#%06x", static_cast<unsigned>(f & 0xffffff));
      out += "color:";
      out += code;
      out += ';';
    }
    if (b != NoColor) {
      snprintf(code, sizeof(code), "#%06x", static_cast<unsigned>(b & 0xffffff));
      out += "background-color:";
      out += code;
      out += ';';
    }
    out += "\">";
  };
  uint8_t current = 0;
  char delimiter = '*';
  uint16_t link = 0;
  std::string linkText;
  auto closeStyle = [&]() {
    if (current & 4)
      out += '`';
    else {
      if (current & 2) out += delimiter;
      if (current & 1) out += std::string(2, delimiter);
      if (current & 16) out += "~~";
      if (current & 8) out += "</u>";
    }
    current = 0;
  };
  auto openStyle = [&](uint8_t s) {
    if (s & 4)
      out += '`';
    else {
      if (s & 8) out += "<u>";
      if (s & 16) out += "~~";
      if (s & 1) out += std::string(2, delimiter);
      if (s & 2) out += delimiter;
    }
    current = s;
  };
  auto closeLink = [&]() {
    if (!link) return;
    const auto& l = links[link - 1];
    if (l.wiki)
      out += "]]";
    else {
      out += "](";
      out.append(l.target.data(), l.target.size());
      out += ')';
    }
    link = 0;
  };
  for (size_t i = 0; i < line.glyphs.size(); ++i) {
    const auto& g = line.glyphs[i];
    uint8_t style = g.style;
    // CommonMark emphasis cannot begin/end with whitespace. Keep separator
    // whitespace within an already open run only when the same style resumes.
    if (g.code == ' ' || g.code == '\t') {
      size_t j = i;
      while (j < line.glyphs.size() && (line.glyphs[j].code == ' ' || line.glyphs[j].code == '\t'))
        ++j;
      if (!current || g.foreground != fore || g.background != back || j == line.glyphs.size() ||
          line.glyphs[j].style != current || line.glyphs[j].foreground != g.foreground ||
          line.glyphs[j].background != g.background)
        style = 0;
    }
    if (g.link != link) {
      closeStyle();
      closeColor();
      closeLink();
      if (g.link && g.link <= links.size()) {
        link = g.link;
        const auto& l = links[link - 1];
        if (l.wiki) {
          out += "[[";
          out.append(l.target.data(), l.target.size());
          out += '|';
        } else out += '[';
      }
    }
    if (g.foreground != fore || g.background != back) {
      closeStyle();
      closeColor();
      openColor(g.foreground, g.background);
    }
    if (style != current) {
      uint8_t previous = current;
      closeStyle();
      delimiter = '*';
      size_t next = i;
      while (next < line.glyphs.size() && line.glyphs[next].style == style) ++next;
      if (style == 3 && previous && next < line.glyphs.size() && (line.glyphs[next].style & 3))
        delimiter = '_';
      openStyle(style);
    }
    // Complete links typed in the editor are standard Markdown / wikilinks.
    // Partial delimiters remain literal until a closing pair exists.
    size_t close = i;
    if (!g.link && g.code == '[' && i + 1 < line.glyphs.size()) {
      bool wiki = line.glyphs[i + 1].code == '[';
      if (wiki) {
        for (size_t j = i + 2; j + 1 < line.glyphs.size(); ++j)
          if (line.glyphs[j].code == ']' && line.glyphs[j + 1].code == ']') {
            close = j + 1;
            break;
          }
      } else {
        for (size_t j = i + 1; j + 1 < line.glyphs.size(); ++j)
          if (line.glyphs[j].code == ']' && line.glyphs[j + 1].code == '(') {
            for (size_t k = j + 2; k < line.glyphs.size(); ++k)
              if (line.glyphs[k].code == ')') {
                close = k;
                break;
              }
            break;
          }
      }
    }
    if (close > i) {
      for (; i <= close; ++i) {
        if (offsets) offsets->push_back(out.size());
        out += utf8(line.glyphs[i].code);
      }
      --i;
    } else {
      if (offsets) offsets->push_back(out.size());
      out += (line.literal || (style & 4)) ? utf8(g.code) : escape(g.code);
    }
  }
  closeStyle();
  closeColor();
  closeLink();
  return out;
}
std::string RichText::markdown(std::vector<size_t>* offsets) const {
  std::string out;
  if (offsets) offsets->clear();
  for (auto& l : lines) {
    size_t base = out.size();
    if (l.dirty) {
      std::vector<size_t> local;
      std::string body = inlineMarkdown(l, offsets ? &local : nullptr);
      out += l.prefix + body + l.ending;
      if (offsets)
        for (size_t n : local) offsets->push_back(base + l.prefix.size() + n);
    } else {
      out.append(l.original.data(), l.original.size());
      if (offsets)
        for (auto& g : l.glyphs) offsets->push_back(base + g.source - l.source);
    }
    if (offsets && !l.ending.empty()) offsets->push_back(out.size() - l.ending.size());
  }
  if (offsets) offsets->push_back(out.size());
  return out;
}
std::vector<Heading> headings(const std::string& s) {
  std::vector<Heading> out;
  bool fence = false;
  for (size_t p = 0; p < s.size();) {
    size_t e = s.find('\n', p);
    if (e == std::string::npos) e = s.size();
    if (s.compare(p, 3, "```") == 0 || s.compare(p, 3, "~~~") == 0) fence = !fence;
    size_t n = 0;
    while (p + n < e && n < 6 && s[p + n] == '#') ++n;
    if (!fence && n && p + n < e && s[p + n] == ' ') {
      size_t finish = e;
      while (finish > p + n + 1 && (s[finish - 1] == '\r' || s[finish - 1] == ' ')) --finish;
      out.push_back({p, static_cast<uint8_t>(n), s.substr(p + n + 1, finish - p - n - 1)});
      if (out.size() >= 4096) break;
    }
    p = e < s.size() ? e + 1 : e;
  }
  return out;
}
std::string anchorName(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    if (c == ' ' || c == '\t')
      out += '-';
    else if (c >= 128 || std::isalnum(c) || c == '_' || c == '-')
      out += char(c < 128 ? std::tolower(c) : c);
  }
  return out;
}
}  // namespace writing
