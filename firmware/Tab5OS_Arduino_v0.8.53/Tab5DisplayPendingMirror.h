#pragma once
#include "Tab5DisplayCacheRanges.h"

// The old front/back differences are physical spans. New writes remove their
// overlap before rendering. Two intervals/row bound storage to 10 KiB at 720p;
// excess fragmentation copies only the remaining old spans, before that write.
namespace Tab5DisplayPendingMirror {
using Span = Tab5DisplayCacheRanges::Row;
struct Row { Span spans[2]; };
static_assert(sizeof(Row) == 8, "Two compact physical spans per row");
inline void reset(Row& row, uint16_t first, uint16_t end) {
  row = {{{first, end}, {0, 0}}};
}
template<class Copy>
void flush(Row& row, Copy copy) {
  for (const auto& span : row.spans)
    if (span.first < span.end) copy(span.first, span.end);
  row = {};
}
template<class Copy>
void overwrite(Row& row, uint16_t first, uint16_t end, Copy copy) {
  if (first >= end) return;
  Span next[4]{};
  unsigned count = 0;
  for (const auto& span : row.spans) {
    if (span.first >= span.end) continue;
    if (end <= span.first || first >= span.end) next[count++] = span;
    else {
      if (span.first < first) next[count++] = {span.first, first};
      if (span.end > end) next[count++] = {end, span.end};
    }
  }
  row = {};
  if (count <= 2) {
    for (unsigned i = 0; i < count; ++i) row.spans[i] = next[i];
  } else {
    // No subsequent mirror may overwrite a pixel from an earlier new write.
    for (unsigned i = 0; i < count; ++i) copy(next[i].first, next[i].end);
  }
}
} // namespace Tab5DisplayPendingMirror
