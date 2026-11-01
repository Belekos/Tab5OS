#pragma once
#include "EpubPageIndex.h"
#include <esp_heap_caps.h>
#include <new>

// UI-task-owned pagination metadata. It is neither DMA data nor an ISR input.
// Keep its storage only while a book is open; never allocate in a constructor.
template <size_t Capacity> class Tab5EpubPageIndex {
 public:
  Tab5EpubPageIndex() = default;
  ~Tab5EpubPageIndex() { clear(); }
  Tab5EpubPageIndex(const Tab5EpubPageIndex&) = delete;
  Tab5EpubPageIndex& operator=(const Tab5EpubPageIndex&) = delete;

  bool reset(uint16_t count, const uint32_t* weights = nullptr) {
    if (!count || count > Capacity) { clear(); return count == 0; }
    if (!index_) {
      void* data = heap_caps_malloc(sizeof(Index), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      // A bounded fallback preserves pagination when external RAM is full.
      // It occupies no more internal RAM than the former permanent index.
      if (!data) data = heap_caps_malloc(sizeof(Index), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      if (!data) return false;
      index_ = new (data) Index;
    }
    index_->reset(count, weights);
    return true;
  }
  void clear() {
    if (!index_) return;
    index_->~Index();
    heap_caps_free(index_);
    index_ = nullptr;
  }
  bool available() const { return index_ != nullptr; }
  uint16_t count() const { return index_ ? index_->count() : 0; }
  uint32_t pages(uint16_t chapter) const { return index_ ? index_->pages(chapter) : 0; }
  bool record(uint16_t chapter, uint32_t pages) { return index_ && index_->record(chapter, pages); }
  uint16_t nextUnknown() const { return index_ ? index_->nextUnknown() : 0; }
  bool complete() const { return index_ && index_->complete(); }
  bool locate(uint32_t page, uint16_t& chapter, uint32_t& local) const {
    return index_ && index_->locate(page, chapter, local);
  }
  epub_pages::Status status(uint16_t chapter, uint32_t localPage, uint64_t done, uint64_t size) const {
    return index_ ? index_->status(chapter, localPage, done, size) : epub_pages::Status{};
  }
 private:
  using Index = epub_pages::Index<Capacity>;
  Index* index_ = nullptr;
};
