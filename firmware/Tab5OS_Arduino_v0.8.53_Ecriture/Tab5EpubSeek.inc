// Whole-book navigation. Dragging only updates the preview; after release one
// page is measured per service call, in the existing background document slot.
// The visible chapter and bookmark survive allocation/measurement failure.
static bool readerSeekPending = false;
static uint16_t readerSeekChapter = 0;
static uint32_t readerSeekPage = 0, readerSeekLastService = 0;
static Tab5PsramTable<RichLoc, READER_PAGE_CACHE_MAX + 1> readerSeekRichStarts;
static Tab5PsramTable<uint32_t, READER_PAGE_CACHE_MAX + 1> readerSeekTextStarts;

static void readerSeekCancel() {
  if (readerSeekPending) readerIndexReleaseChapter();
  readerSeekPending = false;
  readerSeekRichStarts.clear(); readerSeekTextStarts.clear();
}

static void readerSeekSync() {
  if (!readerSeekSlider || readerSeekDragging || readerSeekPending) return;
  const auto s = readerBookPages.status(readerChapter, readerPageIndex, 0, 1);
  if (!s.exact || !s.total) { lv_obj_add_state(readerSeekSlider, LV_STATE_DISABLED); return; }
  lv_slider_set_range(readerSeekSlider, 1, static_cast<int32_t>(s.total));
  lv_slider_set_value(readerSeekSlider, s.page, LV_ANIM_OFF);
  lv_obj_remove_state(readerSeekSlider, LV_STATE_DISABLED);
}

static void readerSeekPreview() {
  if (!readerSeekSlider || !readerStatusLabel) return;
  const uint32_t target = lv_slider_get_value(readerSeekSlider);
  String text = String(T("page")) + " " + String(target) + " / " +
                String(lv_slider_get_max_value(readerSeekSlider));
  lv_label_set_text(readerStatusLabel, text.c_str());
}

static void readerSeekRequest() {
  readerSeekDragging = false;
  uint16_t chapter = 0; uint32_t page = 0;
  if (!readerSeekSlider || !readerBookPages.locate(lv_slider_get_value(readerSeekSlider), chapter, page)) return;
  readerSeekCancel();
  if (chapter == readerChapter && page == readerPageIndex) { readerUpdateEpubStatus(); return; }
  if (page >= READER_PAGE_CACHE_MAX) { toast("Mémoire insuffisante pour la pagination", 3000); readerUpdateEpubStatus(); return; }
  readerIndexReleaseChapter();
  readerSeekChapter = chapter; readerSeekPage = page;
  readerSeekPending = true; readerSeekLastService = 0;
  readerSeekPreview();
}

static void readerSeekEvent(lv_event_t* event) {
  const auto code = lv_event_get_code(event);
  if (code == LV_EVENT_PRESSED || code == LV_EVENT_VALUE_CHANGED) {
    if (code == LV_EVENT_PRESSED) readerSeekCancel();
    readerSeekDragging = true; readerSeekPreview();
  } else if (code == LV_EVENT_RELEASED) readerSeekRequest();
  else if (code == LV_EVENT_PRESS_LOST || code == LV_EVENT_DEFOCUSED) {
    readerSeekDragging = false; readerUpdateEpubStatus();
  } else if (code == LV_EVENT_KEY) {
    const uint32_t key = lv_event_get_key(event);
    if (key == LV_KEY_ENTER) readerSeekRequest();
    else if (key == LV_KEY_ESC) { readerSeekDragging = false; readerSeekCancel(); readerUpdateEpubStatus(); }
  }
}

static bool readerSeekService() {
  if (!readerSeekPending) return false;
  if (readerSeekDragging) return true;
  const uint32_t now = millis();
  if (now - readerSeekLastService < 5) return true;
  readerSeekLastService = now;
  auto failed = [](const char* message = "Mémoire insuffisante pour la pagination") {
    readerSeekCancel(); readerSeekDragging = false;
    toast(message, 3000); readerUpdateEpubStatus();
  };
  if (!readerIndexLoaded) {
    const uint32_t cached = readerRichDoc.blockCount ? readerRichPageStartCount : readerPageStartCount;
    if (readerSeekChapter == readerChapter && readerSeekPage + 1 < cached) {
      const uint16_t previousPage = readerPageIndex;
      readerPageIndex = readerSeekPage; readerSeekCancel();
      if (readerRenderEpubPage()) markReaderProgressDirty();
      else { readerPageIndex = previousPage; readerRenderEpubPage(); toast("Mémoire insuffisante pour la pagination", 3000); }
      return true;
    }
    if (!readerIndexLoadChapter(readerSeekChapter)) { failed("Impossible de lire la section"); return true; }
    // Allocate the complete bounded destination before touching the visible page.
    if (readerIndexDocument.blockCount) {
      if (!readerSeekRichStarts.ensure(readerSeekPage + 2)) { failed(); return true; }
      readerSeekRichStarts[0] = RichLoc{0, 0};
    } else {
      if (!readerSeekTextStarts.ensure(readerSeekPage + 2)) { failed(); return true; }
      ReaderIndexBorrow borrow;
      readerIndexOffset = readerSkipPageWhitespace(0);
      readerSeekTextStarts[0] = readerIndexOffset;
    }
    return true;
  }
  bool advanced = false;
  {
    ReaderIndexBorrow borrow;
    if (readerIndexDocument.blockCount) {
      const RichLoc next = richComputeNextPageStart(readerIndexLoc);
      advanced = !richLocLE(next, readerIndexLoc);
      readerIndexLoc = next;
      readerSeekRichStarts[readerIndexPageCount + 1] = next;
    } else {
      size_t next = readerSkipPageWhitespace(readerIndexOffset);
      if (next >= readerIndexTextLen) advanced = true;
      else {
        if (!readerImageMarkerAt(next, nullptr, &next)) next = readerSkipPageWhitespace(readerFindTextPageEnd(next));
        advanced = next > readerIndexOffset;
      }
      readerIndexOffset = next;
      readerSeekTextStarts[readerIndexPageCount + 1] = next;
    }
  }
  if (!advanced) { failed(); return true; }
  const bool reachedEnd = readerIndexDocument.blockCount ?
      readerIndexLoc.block >= readerIndexDocument.blockCount : readerIndexOffset >= readerIndexTextLen;
  if (reachedEnd && readerIndexPageCount < readerSeekPage) { failed("Impossible de lire la section"); return true; }
  if (readerIndexPageCount++ < readerSeekPage) return true;
  const bool rich = readerIndexDocument.blockCount != 0;
  // Retain the previous chapter and boundaries until the target has actually
  // rendered. A successful preparation is not yet a successful page change.
  Tab5PsramTable<RichLoc, READER_PAGE_CACHE_MAX + 1> previousRichStarts;
  Tab5PsramTable<uint32_t, READER_PAGE_CACHE_MAX + 1> previousTextStarts;
  const bool previousRich = readerRichDoc.blockCount != 0;
  if (previousRich ? !previousRichStarts.ensure(readerRichPageStartCount) : !previousTextStarts.ensure(readerPageStartCount)) {
    failed(); return true;
  }
  if (previousRich) for (uint16_t i = 0; i < readerRichPageStartCount; ++i) previousRichStarts[i] = readerRichPageStarts[i];
  else for (uint16_t i = 0; i < readerPageStartCount; ++i) previousTextStarts[i] = readerPageStarts[i];
  if (rich ? !readerRichPageStarts.ensure(readerSeekPage + 2) : !readerPageStarts.ensure(readerSeekPage + 2)) {
    failed(); return true;
  }
  epub_render::Document previousDoc = readerRichDoc;
  char* previousText = readerText; const size_t previousLen = readerTextLen;
  const uint16_t previousChapter = readerChapter, previousPage = readerPageIndex;
  const uint16_t previousRichCount = readerRichPageStartCount, previousTextCount = readerPageStartCount;
  const bool previousRichComplete = readerRichPaginationComplete, previousTextComplete = readerChapterPaginationComplete;
  const float previousAspect = readerDisplayedAspect;
  readerText = nullptr; freeReaderText();
  readerRichDoc = readerIndexDocument; readerIndexDocument = epub_render::Document{};
  readerText = readerIndexText; readerTextLen = readerIndexTextLen;
  readerIndexText = nullptr; readerIndexTextLen = 0;
  readerChapter = readerSeekChapter; readerPageIndex = readerSeekPage;
  if (rich) {
    for (uint32_t i = 0; i < readerSeekPage + 2; ++i) readerRichPageStarts[i] = readerSeekRichStarts[i];
    readerRichPageStartCount = readerSeekPage + 2;
    readerRichPaginationComplete = readerIndexLoc.block >= readerRichDoc.blockCount;
  } else {
    for (uint32_t i = 0; i < readerSeekPage + 2; ++i) readerPageStarts[i] = readerSeekTextStarts[i];
    readerPageStartCount = readerSeekPage + 2; readerRichPageStartCount = 0;
    readerChapterPaginationComplete = readerIndexOffset >= readerTextLen;
  }
  readerApplyEpubGeometry(readerIndexAspect);
  readerSeekCancel(); readerResumePending = false;
  if (readerRenderEpubPage()) {
    previousDoc.clear(); if (previousText) heap_caps_free(previousText);
    markReaderProgressDirty();
  } else {
    readerRichDoc.clear(); freeReaderText();
    readerRichDoc = previousDoc; readerText = previousText; readerTextLen = previousLen;
    readerChapter = previousChapter; readerPageIndex = previousPage;
    readerRichPageStartCount = previousRichCount; readerPageStartCount = previousTextCount;
    readerRichPaginationComplete = previousRichComplete; readerChapterPaginationComplete = previousTextComplete;
    if (previousRich) for (uint16_t i = 0; i < previousRichCount; ++i) readerRichPageStarts[i] = previousRichStarts[i];
    else for (uint16_t i = 0; i < previousTextCount; ++i) readerPageStarts[i] = previousTextStarts[i];
    readerApplyEpubGeometry(previousAspect);
    readerRenderEpubPage(); // If this also fails, the old bookmark is still intact.
    toast("Mémoire insuffisante pour la pagination", 3000);
  }
  return true;
}
