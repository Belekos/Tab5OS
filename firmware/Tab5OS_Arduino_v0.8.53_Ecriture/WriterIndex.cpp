#include "WriterIndex.h"

#include <algorithm>
#include <cctype>

namespace writing {
void DocumentIndex::reset() {
  chunks_.clear();
  suffix_.clear();
  titles.clear();
  words_ = characters_ = scanned_ = 0;
  complete_ = false;
}
void DocumentIndex::finishLine(State& s, size_t end, Chunk* chunk) {
  const auto& p = s.prefix;
  if (s.prefixSize >= 3 &&
      ((p[0] == '`' && p[1] == '`' && p[2] == '`') || (p[0] == '~' && p[1] == '~' && p[2] == '~')))
    s.fenced = !s.fenced;
  size_t n = 0;
  while (n < s.prefixSize && n < 6 && p[n] == '#') ++n;
  if (!s.fenced && n && n < s.prefixSize && p[n] == ' ' && s.headingCount < 4096) {
    ++s.headingCount;
    if (chunk) {
      std::string title(p.data() + n + 1, s.prefixSize - n - 1);
      while (!title.empty() && (title.back() == '\r' || title.back() == ' ')) title.pop_back();
      while (!title.empty() && !validUtf8(title.data(), title.size())) title.pop_back();
      chunk->titles.push_back({end - s.column, static_cast<uint8_t>(n), title});
    }
  }
  s.column = 0;
  s.prefixSize = 0;
}
void DocumentIndex::consume(const Document& doc, State& s, char c, size_t pos, Chunk* chunk) {
  const auto u = static_cast<unsigned char>(c);
  const bool first = (u & 0xc0) != 0x80;
  bool separator = u <= 32 || (u == 0xc2 && static_cast<unsigned char>(doc.at(pos + 1)) == 0xa0) ||
                   (u == 0xe2 && static_cast<unsigned char>(doc.at(pos + 1)) == 0x80 &&
                    static_cast<unsigned char>(doc.at(pos + 2)) == 0xaf);
  bool word = first ? !separator : s.word;
  if (chunk) {
    if(c=='\n')++chunk->newlines;
    if (first && word && !s.word) ++chunk->words;
    if ((static_cast<unsigned char>(c) & 0xc0) != 0x80) ++chunk->characters;
  }
  s.word = word;
  if (c == '\n')
    finishLine(s, pos, chunk);
  else {
    if (s.prefixSize < s.prefix.size()) s.prefix[s.prefixSize++] = c;
    ++s.column;
  }
}
void DocumentIndex::edited(size_t offset, size_t removed, size_t inserted) {
  // Preserve valid cached tails even when several HID reports arrive in one tick.
  Chunks previous;
  previous.swap(suffix_);
  auto first = std::find_if(chunks_.begin(), chunks_.end(),
                            [&](const Chunk& c) { return c.end > offset || c.end == offset; });
  auto last = std::find_if(first, chunks_.end(), [&](const Chunk& c) {
    return c.begin >= offset + removed && c.begin > offset;
  });
  // Indexed chunks precede every pending suffix chunk, even after a burst
  // of edits. Concatenate the already ordered ranges instead of sorting large
  // State records on every keystroke.
  Chunks candidates;
  candidates.reserve(static_cast<size_t>(chunks_.end() - last) + previous.size());
  for (auto i = last; i != chunks_.end(); ++i) candidates.push_back(std::move(*i));
  for (auto& c : previous) candidates.push_back(std::move(c));
  suffix_.reserve(candidates.size());
  const auto delta = static_cast<ptrdiff_t>(inserted) - static_cast<ptrdiff_t>(removed);
  for (auto i = candidates.begin(); i != candidates.end(); ++i) {
    if (i->begin < offset + removed || i->begin <= offset) continue;
    i->begin = static_cast<size_t>(static_cast<ptrdiff_t>(i->begin) + delta);
    i->end = static_cast<size_t>(static_cast<ptrdiff_t>(i->end) + delta);
    for (auto& h : i->titles)
      h.offset = static_cast<size_t>(static_cast<ptrdiff_t>(h.offset) + delta);
    suffix_.push_back(std::move(*i));
  }
  chunks_.erase(first, chunks_.end());
  titles.erase(std::remove_if(titles.begin(), titles.end(),
                              [&](Heading& h) {
                                if (h.offset >= offset && h.offset < offset + removed) return true;
                                if (h.offset >= offset + removed)
                                  h.offset =
                                      static_cast<size_t>(static_cast<ptrdiff_t>(h.offset) + delta);
                                return false;
                              }),
               titles.end());
  complete_ = false;
}
bool DocumentIndex::step(const Document& doc, size_t budget) {
  if (complete_ || !budget) return complete_;
  size_t pos = chunks_.empty() ? 0 : chunks_.back().end;
  State state = chunks_.empty() ? State{} : chunks_.back().after;
  size_t consumed = 0;
  while (consumed < suffix_.size() && suffix_[consumed].begin == pos) {
    auto& cached = suffix_[consumed++];
    // A mismatching state invalidates this chunk only; later chunks remain
    // candidates for convergence after the replacement chunk is scanned.
    if (!(cached.before == state)) break;
    chunks_.push_back(std::move(cached));
    pos = chunks_.back().end;
    state = chunks_.back().after;
  }
  // One compaction, not one erase(begin) per reused chunk. The latter shifted
  // the entire remaining suffix repeatedly: quadratic work on a large file.
  if (consumed) suffix_.erase(suffix_.begin(), suffix_.begin() + consumed);
  if (pos < doc.size()) {
    Chunk chunk;
    chunk.begin = pos;
    chunk.before = state;
    size_t end = std::min(doc.size(), pos + std::min<size_t>(budget, 4096));
    if (!suffix_.empty()) end = std::min(end, suffix_.front().begin);
    while (pos < end) {
      consume(doc, state, doc.at(pos), pos, &chunk);
      ++pos;
      ++scanned_;
    }
    chunk.end = pos;
    chunk.after = state;
    if (pos == doc.size() && state.column) finishLine(state, pos, &chunk);
    chunks_.push_back(std::move(chunk));
  }
  if (pos == doc.size()) {
    complete_ = true;
    publish();
  }
  return complete_;
}
void DocumentIndex::publish() {
  words_ = characters_ = 0;
  size_t count = 0;
  for (const auto& c : chunks_) {
    words_ += c.words;
    characters_ += c.characters;
    for (const auto& h : c.titles) {
      if (count >= 4096) break;
      if (count < titles.size()) {
        auto& dst = titles[count];
        dst.offset = h.offset;
        dst.level = h.level;
        // Most edits only shift byte offsets. Keep the existing PSRAM string
        // and its capacity instead of destroying/reallocating every title.
        if (dst.title != h.title) dst.title.assign(h.title.data(), h.title.size());
      } else {
        titles.push_back(h);
      }
      ++count;
    }
  }
  titles.erase(titles.begin() + count, titles.end());
}
size_t DocumentIndex::lineAt(const Document& doc,size_t offset) const {
 size_t pos=0,number=1;offset=std::min(offset,doc.size());
 for(const auto& c:chunks_){if(c.end>offset)break;number+=c.newlines;pos=c.end;}
 while(pos<offset){if(doc.at(pos++)=='\n')++number;}return number;
}
bool DocumentIndex::fencedAt(const Document& doc, size_t offset) const {
  State state;
  size_t pos = 0;
  auto after = std::upper_bound(chunks_.begin(), chunks_.end(), offset,
                               [](size_t at, const Chunk& c) { return at < c.end; });
  if (after != chunks_.begin()) {
    const auto& c = *std::prev(after);
    pos = c.end;
    state = c.after;
  }
  while (pos < offset && pos < doc.size()) {
    consume(doc, state, doc.at(pos), pos, nullptr);
    ++pos;
  }
  return state.fenced;
}
}  // namespace writing
