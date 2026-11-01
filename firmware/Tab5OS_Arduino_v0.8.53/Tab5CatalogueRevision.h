#pragma once
#include <atomic>
#include <cstdint>

// Writers already hold the catalogue's mutex or exclusive storage recovery. Zero means a rebuild is in
// progress; readers never publish a model assembled across two revisions.
inline uint32_t tab5CatalogueRevision(const std::atomic<uint32_t>& revision) {
  const auto value=revision.load(std::memory_order_acquire);
  return value&1U?0:value;
}
class Tab5CatalogueChange {
 public:
  explicit Tab5CatalogueChange(std::atomic<uint32_t>& revision):revision_(revision) {
    revision_.fetch_add(1,std::memory_order_acq_rel);
  }
  ~Tab5CatalogueChange() {
    if(revision_.fetch_add(1,std::memory_order_release)==UINT32_MAX)
      revision_.store(2,std::memory_order_release);
  }
  Tab5CatalogueChange(const Tab5CatalogueChange&)=delete;
  Tab5CatalogueChange& operator=(const Tab5CatalogueChange&)=delete;
 private:
  std::atomic<uint32_t>& revision_;
};
