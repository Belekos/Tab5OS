#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "WriterAllocator.h"

namespace writing {
// A bounded editing window. Offsets exposed by this class count Unicode scalars.
// Markdown remains the on-disk format; unchanged lines retain their exact bytes.
constexpr uint32_t NoColor = 0xffffffffu;
struct RichGlyph {
  uint32_t code = 0;
  uint8_t style = 0;
  uint16_t link = 0;
  size_t source = 0;
  uint32_t foreground = NoColor, background = NoColor;
};
struct RichLine {
  ExternalVector<RichGlyph> glyphs;
  std::string prefix, ending;
  // One retained copy per source line. Hundreds of short Markdown lines must
  // not silently consume the internal heap through std::string's allocator.
  IndexString original;
  size_t source = 0;
  uint8_t heading = 0;
  bool literal = false, dirty = false;
  uint8_t block = 0;  // 1: GFM table, 2: standalone Markdown image; atomic in the text flow.
  std::vector<std::vector<std::string>> cells;
  std::vector<uint8_t> alignment;
  std::string imagePath, imageAlt;
};
struct RichLink {
  IndexString target;
  bool wiki = false;
  RichLink() = default;
  RichLink(const std::string& value, bool isWiki)
      : target(value.data(), value.size()), wiki(isWiki) {}
};
class RichText {
 public:
  ExternalVector<RichLine> lines;
  ExternalVector<RichLink> links;
  void parse(const std::string& source, bool fenced = false, bool raw = false);
  void parseCell(const std::string& source);
  std::string cellMarkdown() const;
  std::string markdown(std::vector<size_t>* offsets = nullptr) const;
  std::string plain(size_t a, size_t b) const;
  std::string fragment(size_t a, size_t b) const;
  size_t size() const;
  RichGlyph glyph(size_t index) const;
  size_t sourceAt(size_t index) const;
  size_t indexAtSource(size_t source) const;
  uint8_t styleAt(size_t index) const;
  bool allStyle(size_t a, size_t b, uint8_t bit) const;
  void toggle(size_t a, size_t b, uint8_t bit);
  size_t replace(size_t a, size_t b, const std::string& text, uint8_t style,
                 uint32_t foreground = NoColor, uint32_t background = NoColor);
  void color(size_t a, size_t b, uint32_t color, bool background);
  void heading(size_t index);
  const RichLine* blockAt(size_t index) const;
  static std::string utf8(uint32_t code);

 private:
  void locate(size_t index, size_t& line, size_t& column) const;
  std::string inlineMarkdown(const RichLine& line, std::vector<size_t>* offsets = nullptr) const;
};
std::string tableMarkdown(const std::vector<std::vector<std::string>>& cells,
                          const std::vector<uint8_t>& alignment = {});
struct Heading {
  size_t offset;
  uint8_t level;
  IndexString title;
  Heading(size_t at, uint8_t depth, const std::string& text)
      : offset(at), level(depth), title(text.data(), text.size()) {}
};
std::vector<Heading> headings(const std::string& source);
std::string anchorName(const std::string& title);
}  // namespace writing
