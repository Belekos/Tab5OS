// Included after richCreateSpanGroup: measure with the very same LVGL span
// engine and styles used for drawing. A single temporary group is reused for
// all candidate ends. Only candidate-page text is copied/shaped, never the
// whole long paragraph. No per-word widgets or per-candidate widget rebuilds.
static bool richTextLayoutError = false;
class RichTextMeasure {
 public:
  RichTextMeasure(const epub_render::Block& block, uint32_t start, int width)
      : block_(block), start_(start) {
#if LV_USE_SPAN
    if (!readerMeasureHost || !block.runCount) return;
    spans_ = static_cast<lv_span_t**>(heap_caps_calloc(block.runCount, sizeof(lv_span_t*),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!spans_) return;
    group_ = richCreateSpanGroup(readerMeasureHost, block, start, width);
    if (!group_) return;
    lv_obj_update_layout(group_);
    width_ = lv_obj_get_content_width(group_);
#else
    (void)width;
#endif
  }
  ~RichTextMeasure() {
    if (group_) lv_obj_delete(group_);
    if (spans_) heap_caps_free(spans_);
  }
  int height(uint32_t end) {
#if LV_USE_SPAN
    if (end <= start_) return 0;
    if (!group_ || !spans_ || width_ < 1) return failure();
    uint32_t logical = 0;
    for (uint16_t i = 0; i < block_.runCount; ++i) {
      const auto& run = readerRichDoc.runs[block_.runStart + i];
      const uint32_t begin = logical;
      logical += run.textLen;
      const bool active = logical > start_ && begin < end;
      if (active && !spans_[i]) {
#if LVGL_VERSION_MINOR >= 4
        spans_[i] = lv_spangroup_add_span(group_);
#else
        spans_[i] = lv_spangroup_new_span(group_);
#endif
        if (!spans_[i]) return failure();
        auto* style = lv_span_get_style(spans_[i]);
        lv_style_set_text_font(style, richFontFor(run.style.fontPx, run.style.flags));
      }
      if (!spans_[i]) continue;
      if (!active) {
        if (lastEnd_ > begin) lv_span_set_text(spans_[i], "");
        if (!lv_span_get_text(spans_[i])) return failure();
        continue;
      }
      if (lastEnd_ >= logical && end >= logical) continue;
      char* text = const_cast<char*>(readerRichDoc.ptr(run.textOff));
      char* cut = end < logical ? text + end - begin : nullptr;
      const char saved = cut ? *cut : 0;
      if (cut) *cut = 0;
      // Use the same setter as rendering: with Arabic/Persian enabled even
      // LVGL's "static" setter shapes and copies its argument immediately.
      lv_span_set_text(spans_[i], text + (start_ > begin ? start_ - begin : 0));
      if (cut) *cut = saved;
      if (!lv_span_get_text(spans_[i])) return failure();
    }
    lastEnd_ = end;
    // get_expand_height does not trust the object's previous cached height.
    const int result = lv_spangroup_get_expand_height(group_, width_);
    return result > 0 ? result : 0;
#else
    (void)end; return failure();
#endif
  }
 private:
  static int failure() { richTextLayoutError = true; return 0x3fffffff; }
  const epub_render::Block& block_;
  uint32_t start_;
  uint32_t lastEnd_ = 0;
  lv_obj_t* group_ = nullptr;
  lv_span_t** spans_ = nullptr;
  int width_ = 0;
};

static int richLayoutText(const epub_render::Block& b, uint32_t startOff,
                          uint32_t endOff, int width, int maxH,
                          uint32_t* fitEnd) {
  if (fitEnd) *fitEnd = startOff;
  if (startOff >= endOff || startOff >= b.textLen || width < 40 || maxH <= 0) return 0;
  if (endOff > b.textLen) endOff = b.textLen;
  RichTextMeasure measure(b, startOff, width);

  // Bound the first pass to roughly a page. A very long paragraph must not be
  // fully laid out on every page turn or every background indexing step.
  uint32_t low = startOff;
  uint32_t high = richUtf8BoundaryBack(b, min(endOff, startOff + 2048U), startOff);
  while (measure.height(high) <= maxH) {
    low = high;
    if (high == endOff) {
      if (fitEnd) *fitEnd = high;
      return measure.height(high);
    }
    high = richUtf8BoundaryBack(b, min(endOff, high + (high - startOff)), low);
  }
  if (richTextLayoutError) return 0;
  // Binary search on UTF-8 boundaries, using actual LVGL wrapping at every
  // candidate. This includes CJK, long words and mixed-font inline runs.
  while (high > low + 1U) {
    uint32_t mid = richUtf8BoundaryBack(b, low + (high - low) / 2U, low);
    if (mid <= low) break;
    if (measure.height(mid) <= maxH) low = mid;
    else high = mid;
    if (richTextLayoutError) return 0;
  }
  if (low == startOff) return 0; // Caller starts the block on the next page.

  // Prefer a whole-word break for ordinary prose. A word/sequence longer than
  // a page (including unspaced CJK) still advances at a complete UTF-8 codepoint.
  if (low < endOff && !richSpace(richByteAt(b, low)) && !richSpace(richByteAt(b, low - 1))) {
    const uint32_t wordStart = richPrevWordBoundary(b, low, startOff);
    if (wordStart > startOff) low = wordStart;
  }
  if (fitEnd) *fitEnd = low;
  return measure.height(low);
}
