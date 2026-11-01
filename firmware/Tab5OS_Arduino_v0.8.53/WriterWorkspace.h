#pragma once
#include "WriterStorage.h"

namespace writing {
// Only document membership and view positions live here; documents remain lazy.
class Workspace {
 public:
  static constexpr size_t MaxEntries = 256, MaxProjects = 8, MaxExclusions = 256;
  static constexpr size_t PathSize = 300;
  struct Entry {
    char path[PathSize] = {};
    uint8_t projectID = 0; // 0 is the standalone documents group.
    uint8_t syntaxLanguage = 0; // SyntaxLanguage::Auto; stored per document.
    uint32_t start = 0, cursor = 0, anchor = 0;
    int32_t scroll = 0;
  };
  struct Project { char name[64] = {}, root[PathSize] = {}; uint8_t id = 0; };
  Workspace() = default;
  ~Workspace();
  Workspace(const Workspace&) = delete;
  Workspace& operator=(const Workspace&) = delete;
  bool begin();
  size_t count() const;
  Entry* entry(size_t i);
  const Entry* entry(size_t i) const;
  size_t projectCount() const;
  Project* project(size_t i);
  const Project* project(size_t i) const;
  uint8_t activeProject() const;
  bool validProject(uint8_t id) const;
  bool setActiveProject(uint8_t id);
  int find(const char* path, uint8_t projectID) const;
  int add(const char* path, uint8_t projectID = 0); // Explicit open clears exclusion.
  int addScanned(const char* path, uint8_t projectID); // Never clears an exclusion.
  bool excluded(const char* path, uint8_t projectID) const;
  size_t exclusionCount() const;
  bool remove(size_t i); // Membership only. Never removes a filesystem object.
  uint8_t createProject(const char* name, const char* root = "");
  bool dirty() const { return dirty_; }
  void markDirty() { if (data_) dirty_ = true; }
  bool save(FileSystem& fs, void (*yieldFn)() = nullptr);
  bool load(FileSystem& fs);
  bool busy() const { return busy_; }
  const char* error() const { return error_; }
  // Allocation-free FAT validation, normalisation and comparison.
  static bool normalizePath(const char* source, char out[PathSize]);
  static bool samePath(const char* a, const char* b);
 private:
  struct Data;
  Data* data_ = nullptr;
  bool dirty_ = false, busy_ = false, primaryInvalid_ = false;
  bool readIO_ = false, primaryIO_ = false, temporaryRecovered_ = false;
  const char* error_ = "";
  int addImpl(const char*, uint8_t, bool);
  bool readFile(FileSystem&, const char*, Data&);
  bool valid(const Data&) const;
};
} // namespace writing
