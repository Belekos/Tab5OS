#include "Tab5VolumeFS.h"
#include "Tab5WriterWorkspace.h"
#include "WriterSyntax.h"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <cstring>
#include <new>

namespace writing {
struct WorkspaceScanner::Impl {
  Workspace* workspace = nullptr;
  File directories[9]; // Root plus eight descendant levels, all metadata in PSRAM.
  char paths[9][Workspace::PathSize] = {};
  uint8_t depth = 0, projectID = 0;
  uint32_t visited = 0, lastRound = 0;
  size_t nextProject = 0;
  bool running = false, round = false, wasVisible = false;
  const char* error = "";
  void close() {
    for (auto& directory : directories) if (directory) directory.close();
    depth = 0; running = false;
  }
  bool start(uint8_t id, const char* path, bool updateRoot) {
    char normalized[Workspace::PathSize];
    if (!workspace || !id || !workspace->validProject(id) || !Workspace::normalizePath(path, normalized)) {
      error = "Dossier de projet invalide"; return false;
    }
    File directory = tab5FS.open(normalized, FILE_READ);
    if (!directory || !directory.isDirectory()) {
      if (directory) directory.close();
      error = "Dossier de projet inaccessible"; return false;
    }
    close(); directories[0] = directory; std::strcpy(paths[0], normalized);
    projectID = id; visited = 0; running = true; error = "";
    if (updateRoot) for (size_t p = 0; p < workspace->projectCount(); ++p) {
      auto* project = workspace->project(p);
      if (project->id == id && std::strcmp(project->root, normalized)) {
        std::strcpy(project->root, normalized); workspace->markDirty(); break;
      }
    }
    return true;
  }
  static bool ignoredDirectory(const char* name) {
    if (!name || !*name || *name == '.') return true;
    char folded[Workspace::PathSize]; size_t i = 0;
    for (; name[i] && i + 1 < sizeof(folded); ++i)
      folded[i] = name[i] >= 'A' && name[i] <= 'Z' ? char(name[i] + 32) : name[i];
    folded[i] = 0;
    return !std::strcmp(folded, "node_modules") || !std::strcmp(folded, "__pycache__") ||
      !std::strcmp(folded, "system volume information") || !std::strcmp(folded, "$recycle.bin");
  }
  void step() {
    for (unsigned budget = 0; running && budget < 4; ++budget) {
      if (visited >= 20000) { error = "Analyse limitée à 20 000 entrées"; close(); break; }
      File item = directories[depth].openNextFile();
      if (!item) {
        directories[depth].close();
        if (depth) --depth; else running = false;
        continue;
      }
      ++visited;
      const char* name = item.name();
      if (name) { const char* slash = std::strrchr(name, '/'); if (slash) name = slash + 1; }
      char path[Workspace::PathSize];
      const size_t parentLength = std::strlen(paths[depth]);
      const size_t nameLength = name ? std::strlen(name) : 0;
      const size_t separator = parentLength > 1 ? 1 : 0;
      if (!nameLength || parentLength + separator + nameLength >= sizeof(path)) { item.close(); continue; }
      std::memcpy(path, paths[depth], parentLength);
      if (separator) path[parentLength] = '/';
      std::memcpy(path + parentLength + separator, name, nameLength + 1);
      if (item.isDirectory()) {
        if (depth < 8 && !ignoredDirectory(name)) {
          ++depth; std::strcpy(paths[depth], path); directories[depth] = item;
        } else item.close();
      } else {
        item.close();
        if (editableFile(path)) {
          const int result = workspace->addScanned(path, projectID);
          if (result < 0 && *workspace->error()) {
            error = workspace->error();
            if (workspace->count() >= Workspace::MaxEntries) { close(); break; }
          }
        }
      }
    }
  }
};
WorkspaceScanner::~WorkspaceScanner() {
  if (impl_) { impl_->close(); impl_->~Impl(); release(impl_); }
}
bool WorkspaceScanner::begin(Workspace& workspace) {
  if (!workspace.begin()) return false;
  if (!impl_) {
    void* memory = allocate(sizeof(Impl)); if (!memory) return false;
    impl_ = new (memory) Impl();
  }
  impl_->workspace = &workspace; return true;
}
bool WorkspaceScanner::startScan(uint8_t id, const char* folder) {
  if (!impl_) return false;
  const bool result = impl_->start(id, folder, true);
  if (result) { impl_->round = false; impl_->lastRound = millis(); }
  return result;
}
void WorkspaceScanner::service(uint32_t now, bool visible) {
  if (!impl_) return;
  if (!visible) {
    impl_->close(); impl_->round = false; impl_->wasVisible = false; return;
  }
  if (!impl_->wasVisible || uint32_t(now - impl_->lastRound) >= 30000u) {
    if (!impl_->running) { impl_->round = true; impl_->nextProject = 0; impl_->lastRound = now; }
  }
  impl_->wasVisible = true;
  if (!impl_->running && impl_->round) {
    // Opening directories also has a fixed per-pass budget.
    for (unsigned budget = 0; budget < 4 && impl_->nextProject < impl_->workspace->projectCount(); ++budget) {
      const auto* project = impl_->workspace->project(impl_->nextProject++);
      if (project->root[0] && impl_->start(project->id, project->root, false)) break;
    }
    if (impl_->nextProject >= impl_->workspace->projectCount()) impl_->round = false;
  }
  if (impl_->running) impl_->step();
}
bool WorkspaceScanner::busy() const { return impl_ && (impl_->running || impl_->round); }
bool WorkspaceScanner::storageIdle() const { return !impl_ || !impl_->running; }
const char* WorkspaceScanner::error() const { return impl_ ? impl_->error : "PSRAM insuffisante pour les projets"; }
} // namespace writing
