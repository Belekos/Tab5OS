// Included by the sketch after readerCurrentSectionName(). Uses the very same
// layout functions as the visible reader, on an off-screen content rectangle.
// No LVGL callbacks are dispatched while the measuring state is borrowed.
#include "Tab5EpubPageIndex.h"

static void readerUpdateEpubStatus();
static bool readerSeekService();
static void readerSeekCancel();
static Tab5EpubPageIndex<epub_lite::MAX_CHAPTERS> readerBookPages;
static epub_render::Document readerIndexDocument;
static char* readerIndexText = nullptr;
static size_t readerIndexTextLen = 0;
static lv_obj_t* readerIndexContent = nullptr;
static uint16_t readerIndexChapter = 0;
static RichLoc readerIndexLoc{0, 0};
static size_t readerIndexOffset = 0;
static uint32_t readerIndexPageCount = 0;
static uint32_t readerIndexLastService = 0;
static uint32_t readerIndexLayoutKey = 0;
static bool readerIndexLoaded = false;
static bool readerIndexFailed = false;
static bool readerIndexSaved = false;
static bool readerIndexCacheTried = false;
static float readerIndexAspect = 2.0f / 3.0f;

static uint32_t readerIndexMix(uint32_t hash, uint32_t value) {
  for (uint8_t i = 0; i < 4; ++i) hash = (hash ^ static_cast<uint8_t>(value >> (i * 8))) * 16777619U;
  return hash;
}

static void readerIndexReleaseChapter() {
  readerIndexDocument.clear();
  if (readerIndexText) heap_caps_free(readerIndexText);
  readerIndexText = nullptr; readerIndexTextLen = 0;
  readerIndexLoaded = false;
  readerIndexLoc = RichLoc{0, 0}; readerIndexOffset = 0; readerIndexPageCount = 0;
}

static void readerBookIndexStop(bool releaseIndex = true) {
  readerSeekCancel();
  readerIndexReleaseChapter();
  if (readerIndexContent) lv_obj_delete(readerIndexContent);
  readerIndexContent = nullptr;
  if (releaseIndex) readerBookPages.clear();
}

static String readerBookIndexPath() { return readerProgressPath(readerPath) + ".pages"; }

static uint32_t readerBookLayoutKey() {
  uint32_t key = readerIndexMix(2166136261U, 0x08220001U); // renderer/cache schema
  key = readerIndexMix(key, readerBook.contentFingerprint);
  key = readerIndexMix(key, settings.fontIndex);
  key = readerIndexMix(key, lv_display_get_horizontal_resolution(lvDisplay));
  key = readerIndexMix(key, lv_display_get_vertical_resolution(lvDisplay));
  key = readerIndexMix(key, lv_font_get_line_height(readerFont()));
  key = readerIndexMix(key, static_cast<uint32_t>(readerPageAspect * 1000000.0f));
  // The active regular font is already in memory. Hash its contents so replacing
  // a font with a same-sized font cannot silently reuse a different pagination.
  for (size_t i = 0; ttfData && i < ttfDataSize; ++i) key = (key ^ ttfData[i]) * 16777619U;
  const char* variants[] = {FONT_BOLD_FILE, FONT_ITALIC_FILE, FONT_BOLDITALIC_FILE};
  for (const char* path : variants) {
    File font = tab5FS.open(path, FILE_READ);
    key = readerIndexMix(key, font ? static_cast<uint32_t>(font.size()) : 0);
    key = readerIndexMix(key, font ? static_cast<uint32_t>(font.getLastWrite()) : 0);
    if (font) font.close();
  }
  return key;
}

static void readerBookIndexReadCache() {
  if (!sdReady || !readerBook.contentFingerprint) return;
  File f = tab5FS.open(readerBookIndexPath().c_str(), FILE_READ);
  if (!f) return;
  // Fixed-size binary record: schema, layout signature, section count, counts,
  // checksum. Read completely and validate before accepting any page counts.
  uint32_t record[epub_lite::MAX_CHAPTERS + 4] = {};
  const size_t words = readerBook.chapterCount + 4U;
  const size_t bytes = words * sizeof(uint32_t);
  bool valid = f.size() == bytes && f.read(reinterpret_cast<uint8_t*>(record), bytes) == bytes;
  f.close();
  valid = valid && record[0] == 0x50475032U && record[1] == readerIndexLayoutKey && record[2] == readerBook.chapterCount;
  uint32_t hash = 2166136261U;
  for (size_t i = 0; i + 1 < words; ++i) hash = readerIndexMix(hash, record[i]);
  valid = valid && hash == record[words - 1];
  for (uint16_t i = 0; valid && i < readerBook.chapterCount; ++i)
    valid = record[i + 3] > 0 && record[i + 3] <= 1000000U;
  if (!valid) return;
  for (uint16_t i = 0; i < readerBook.chapterCount; ++i) readerBookPages.record(i, record[i + 3]);
  readerIndexSaved = true;
}

static void readerBookIndexWriteCache() {
  if (readerIndexSaved || readerIndexCacheTried || !readerBookPages.complete() || !sdReady || !readerBook.contentFingerprint) return;
  readerIndexCacheTried = true; // A full/read-only card must not trigger a write every loop.
  if (!tab5FS.exists(PROGRESS_DIR) && !tab5FS.mkdir(PROGRESS_DIR)) return;
  uint32_t record[epub_lite::MAX_CHAPTERS + 4] = {};
  record[0] = 0x50475032U; record[1] = readerIndexLayoutKey; record[2] = readerBook.chapterCount;
  const size_t words = readerBook.chapterCount + 4U;
  for (uint16_t i = 0; i < readerBook.chapterCount; ++i) record[i + 3] = readerBookPages.pages(i);
  uint32_t hash = 2166136261U;
  for (size_t i = 0; i + 1 < words; ++i) hash = readerIndexMix(hash, record[i]);
  record[words - 1] = hash;
  const String target = readerBookIndexPath(), temp = target + ".tmp";
  tab5FS.remove(temp.c_str());
  File f = tab5FS.open(temp.c_str(), FILE_WRITE);
  if (!f) return;
  const size_t bytes = words * sizeof(uint32_t);
  const bool ok = f.write(reinterpret_cast<const uint8_t*>(record), bytes) == bytes;
  f.close();
  if (!ok) { tab5FS.remove(temp.c_str()); return; }
  tab5FS.remove(target.c_str());
  if (tab5FS.rename(temp.c_str(), target.c_str())) readerIndexSaved = true;
  else tab5FS.remove(temp.c_str());
}

static void readerBookIndexStart() {
  readerBookIndexStop(false);
  readerIndexFailed = !readerBookPages.reset(readerBook.chapterCount, readerBook.chapterBytes);
  readerIndexSaved = false; readerIndexCacheTried = false; readerIndexLastService = millis();
  if (readerIndexFailed) return;
  readerIndexLayoutKey = readerBookLayoutKey();
  readerBookIndexReadCache();
}

static void readerBookIndexRecordVisible() {
  uint32_t pages = 0;
  if (readerRichDoc.blockCount && readerRichPaginationComplete && readerRichPageStartCount >= 2)
    pages = readerRichPageStartCount - 1U;
  else if (!readerRichDoc.blockCount && readerChapterPaginationComplete)
    pages = readerPageStartCount >= 2 ? readerPageStartCount - 1U : 1U;
  if (!pages) return;
  // A real render always wins over a cached count. Any mismatch invalidates the
  // whole cache rather than mixing counts produced with different font metrics.
  if (readerBookPages.pages(readerChapter) && readerBookPages.pages(readerChapter) != pages) {
    readerIndexReleaseChapter();
    readerBookPages.reset(readerBook.chapterCount, readerBook.chapterBytes);
    readerIndexSaved = false; readerIndexCacheTried = false; readerIndexFailed = false;
  }
  readerBookPages.record(readerChapter, pages);
}

static uint32_t readerBookBlockUnits(const epub_render::Block& block) {
  if (block.kind == epub_render::BlockKind::Image) return 1024U;
  if (block.kind == epub_render::BlockKind::Table) return block.rows ? block.rows * 80U : 1U;
  return block.textLen ? block.textLen : 1U;
}

static epub_pages::Status readerBookIndexStatus() {
  readerBookIndexRecordVisible();
  uint64_t done = 0, size = 0;
  if (readerRichDoc.blockCount && readerPageIndex < readerRichPageStartCount) {
    const RichLoc loc = readerRichPageStarts[readerPageIndex];
    for (uint16_t i = 0; i < readerRichDoc.blockCount; ++i) {
      const auto& block = readerRichDoc.blocks[i]; const uint32_t units = readerBookBlockUnits(block);
      size += units;
      if (i < loc.block) done += units;
      else if (i == loc.block) {
        const uint32_t offset = block.kind == epub_render::BlockKind::Table ? loc.offset * 80U : loc.offset;
        done += offset < units ? offset : units;
      }
    }
  } else {
    size = readerTextLen;
    if (readerPageIndex < readerPageStartCount) done = readerPageStarts[readerPageIndex];
  }
  return readerBookPages.status(readerChapter, readerPageIndex, done, size);
}

static String readerBookIndexStatusText() {
  if (readerIndexFailed && !readerBookPages.available())
    return String(T("Mémoire insuffisante pour la pagination")) + "  ·  " + readerCurrentSectionName();
  const auto status = readerBookIndexStatus();
  String text = status.exact ? "" : "~";
  text += String(status.percent) + "%";
  if (status.pageKnown) {
    text += String("  ·  ") + T("page") + " " + String(status.page) + " / ";
    text += status.exact ? String(status.total) : String("…");
  } else if (!readerIndexFailed) text += String("  ·  ") + T("Mise en page…");
  text += String("  ·  ") + readerCurrentSectionName();
  return text;
}

struct ReaderIndexBorrow {
  epub_render::Document doc = readerRichDoc;
  char* text = readerText;
  size_t len = readerTextLen;
  lv_obj_t* content = readerContent;
  ReaderIndexBorrow() {
    readerRichDoc = readerIndexDocument; readerText = readerIndexText;
    readerTextLen = readerIndexTextLen; readerContent = readerIndexContent;
  }
  ~ReaderIndexBorrow() {
    readerRichDoc = doc; readerText = text; readerTextLen = len; readerContent = content;
  }
};

static bool readerIndexLoadChapter(uint16_t chapter) {
  readerIndexChapter = chapter;
    String err;
    bool rich = epub_render::buildDocument(tab5FS, readerPath.c_str(), readerBook, readerIndexChapter,
        fontSizeForIndex(settings.fontIndex), readerIndexDocument, err);
    int viewportW = readerIndexDocument.viewportW, viewportH = readerIndexDocument.viewportH;
    if (!rich || !readerIndexDocument.blockCount) {
      readerIndexDocument.clear();
      if (!epub_lite::readChapter(tab5FS, readerPath.c_str(), readerBook, readerIndexChapter,
          &readerIndexText, &readerIndexTextLen, err, &viewportW, &viewportH)) {
        readerIndexReleaseChapter(); return false;
      }
    }
    if (!readerIndexContent) {
      readerIndexContent = lv_obj_create(readerScreen);
      lv_obj_remove_style_all(readerIndexContent);
      lv_obj_set_style_pad_all(readerIndexContent, 18, 0);
      lv_obj_remove_flag(readerIndexContent, lv_obj_flag_t(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
      lv_obj_set_pos(readerIndexContent, -4000, -4000);
    }
    float aspect = readerPageAspect;
    if (readerBook.fixedLayout && viewportW > 0 && viewportH > 0) aspect = static_cast<float>(viewportW) / viewportH;
    readerIndexAspect = aspect;
    const auto box = epub_pages::geometry(lv_display_get_horizontal_resolution(lvDisplay),
        lv_display_get_vertical_resolution(lvDisplay), aspect);
    lv_obj_set_size(readerIndexContent, box.width, box.height);
    lv_obj_update_layout(readerIndexContent);
    readerIndexLoaded = true;
    return true;
}

static void readerBookIndexService() {
  if (!readerActive || !readerIsEpub || !readerScreen || !pathStorageReady(readerPath)) return;
  if (readerSeekService() || readerIndexFailed) return;
  const uint32_t now = millis();
  if (now - readerIndexLastService < 20 || now - lastReaderProgressChangeMs < 180) return;
  readerIndexLastService = now;
  if (readerBookPages.complete()) { readerIndexReleaseChapter(); readerBookIndexWriteCache(); return; }
  if (readerIndexLoaded && readerBookPages.pages(readerIndexChapter)) readerIndexReleaseChapter();
  if (!readerIndexLoaded) {
    readerIndexChapter = readerBookPages.nextUnknown();
    if (readerIndexChapter >= readerBook.chapterCount) return;
    if (!readerIndexLoadChapter(readerIndexChapter)) { readerIndexFailed = true; readerUpdateEpubStatus(); return; }
    return; // Never parse a chapter and paginate it in the same loop iteration.
  }
  bool complete = false, advanced = false;
  {
    ReaderIndexBorrow borrowed;
    if (readerIndexDocument.blockCount) {
      const RichLoc next = richComputeNextPageStart(readerIndexLoc);
      advanced = next.block > readerIndexLoc.block || (next.block == readerIndexLoc.block && next.offset > readerIndexLoc.offset);
      complete = next.block >= readerIndexDocument.blockCount;
      readerIndexLoc = next;
    } else {
      size_t next = readerSkipPageWhitespace(readerIndexOffset);
      if (next >= readerIndexTextLen) complete = advanced = true; // An empty section is one displayed page.
      else {
        if (!readerImageMarkerAt(next, nullptr, &next)) next = readerSkipPageWhitespace(readerFindTextPageEnd(next));
        advanced = next > readerIndexOffset;
        complete = next >= readerIndexTextLen;
      }
      readerIndexOffset = next;
    }
  }
  if (!advanced || readerIndexPageCount >= 1000000U) {
    readerIndexReleaseChapter(); readerIndexFailed = true; readerUpdateEpubStatus(); return;
  }
  ++readerIndexPageCount;
  if (complete) {
    readerBookPages.record(readerIndexChapter, readerIndexPageCount);
    readerIndexReleaseChapter();
    readerUpdateEpubStatus();
  }
}
