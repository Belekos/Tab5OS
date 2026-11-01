#pragma once
#include <algorithm>
#include <array>
#include <vector>

#include "WriterDocument.h"
#include "WriterRich.h"

namespace writing {
// Raw text stays in the gap buffer. Only 4 KiB of the structural index is
// scanned per service call; unchanged suffixes are reused after local edits.
class DocumentIndex {
 public:
  std::vector<Heading> titles;
  void reset();
  void edited(size_t offset, size_t removed, size_t inserted);
  bool step(const Document& doc, size_t budget = 4096);
  bool ready() const { return complete_; }
  size_t words() const { return words_; }
  size_t characters() const { return characters_; }
  size_t scannedBytes() const { return scanned_; }
  size_t lineAt(const Document& doc,size_t offset) const;
  bool fencedAt(const Document& doc, size_t offset) const;

 private:
  struct State {
    bool fenced = false, word = false;
    size_t column = 0, headingCount = 0;
    std::array<char, 256> prefix{};
    size_t prefixSize = 0;
    bool operator==(const State& o) const {
      return fenced == o.fenced && word == o.word && column == o.column &&
             headingCount == o.headingCount && prefixSize == o.prefixSize &&
             std::equal(prefix.begin(), prefix.begin() + prefixSize, o.prefix.begin());
    }
  };
  struct Chunk {
    size_t begin = 0, end = 0, words = 0, characters = 0, newlines = 0;
    State before, after;
    std::vector<Heading, ExternalAllocator<Heading>> titles;
  };
  using Chunks = std::vector<Chunk, ExternalAllocator<Chunk>>;
  Chunks chunks_, suffix_;
  size_t words_ = 0, characters_ = 0, scanned_ = 0;
  bool complete_ = false;
  static void consume(const Document& doc, State& state, char c, size_t pos, Chunk* chunk);
  static void finishLine(State& state, size_t end, Chunk* chunk);
  void publish();
};
}  // namespace writing
