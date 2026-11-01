#pragma once
#include <FS.h>
#include <cstddef>

// Boot only, before any SD-backed settings/resources/apps are opened. The
// caller passes native SD, never the virtual FS whose old paths are aliases.
namespace Tab5SystemMigration {
enum class Status { NotNeeded, Renamed, Merged, Failed };
enum class Error {
  None, NotDirectory, ReservedPath, PathTooLong, DepthLimit,
  OpenDirectory, ReadDirectory, Probe, MakeDirectory, Rename, BackupCollision,
  NoBackupName
};
struct Result {
  Status status = Status::NotNeeded;
  Error error = Error::None;
  size_t moved = 0;
  size_t conflicts = 0;
  bool ok() const { return status != Status::Failed; }
};
Result run(fs::FS& nativeSd, void (*cooperate)() = nullptr);
const char* errorName(Error error);
}
