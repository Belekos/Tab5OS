#pragma once
#include "WriterWorkspace.h"

namespace writing {
class WorkspaceScanner {
 public:
  WorkspaceScanner() = default;
  ~WorkspaceScanner();
  WorkspaceScanner(const WorkspaceScanner&) = delete;
  WorkspaceScanner& operator=(const WorkspaceScanner&) = delete;
  bool begin(Workspace& workspace);
  bool startScan(uint8_t projectID, const char* folder);
  // At most four directory entries per pass. Hidden app holds no SD handles.
  void service(uint32_t now, bool visible);
  bool busy() const;
  bool storageIdle() const;
  const char* error() const;
 private:
  struct Impl;
  Impl* impl_ = nullptr;
};
} // namespace writing
