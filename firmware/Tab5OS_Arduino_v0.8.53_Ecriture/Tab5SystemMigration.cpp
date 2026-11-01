#include "Tab5SystemMigration.h"
#include <Arduino.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace Tab5SystemMigration {
namespace {
constexpr const char* OldRoot = "/M5Writer";
constexpr const char* NewRoot = "/System";
constexpr const char* ArchiveRoot = "/System/migration_0824";
constexpr const char* Pending = "/System/migration_0824/pending";
constexpr const char* Legacy = "/System/migration_0824/pending/legacy";
constexpr const char* Replaced = "/System/migration_0824/pending/replaced";
constexpr size_t MaxPathBytes = 1024;
constexpr unsigned MaxDepth = 32;

struct Migration {
  fs::FS& sd;
  void (*cooperate)();
  Result result;
  bool fail(Error error) {
    result.status = Status::Failed; result.error = error; return false;
  }
  bool directory(const String& path) {
    File f = sd.open(path.c_str(), FILE_READ);
    const bool ok = f && f.isDirectory(); f.close();
    return ok || fail(Error::NotDirectory);
  }
  bool exists(const String& path, bool& present) {
    errno = 0;
    present = sd.exists(path.c_str());
    // Arduino's native FAT VFS reports ENOENT for an absent path. Treat
    // unreadable/unknown paths as errors, never as permission to overwrite.
    return present || errno == ENOENT || fail(Error::Probe);
  }
  bool mkdir(const String& path) {
    bool present = false;
    if (!exists(path, present)) return false;
    if (present) return directory(path);
    return sd.mkdir(path.c_str()) || fail(Error::MakeDirectory);
  }
  bool join(const String& parent, const char* name, String& out) {
    const size_t length = parent.length() + 1 + std::strlen(name);
    if (length >= MaxPathBytes) return fail(Error::PathTooLong);
    if (!out.reserve(length + 1)) return fail(Error::PathTooLong);
    out = parent; out += '/'; out += name;
    return out.length() == length || fail(Error::PathTooLong);
  }
  bool parents(const String& path) {
    for (unsigned i = 1; i < path.length(); ++i)
      if (path[i] == '/' && !mkdir(path.substring(0, i))) return false;
    return true;
  }
  bool move(const String& from, const String& to) {
    // Never rely on rename's platform-specific overwrite behavior.
    bool present = false;
    if (!exists(to, present)) return false;
    if (present) return fail(Error::BackupCollision);
    if (!sd.rename(from.c_str(), to.c_str())) return fail(Error::Rename);
    ++result.moved;
    if (cooperate) cooperate();
    return true;
  }
  static bool newResourceWins(const String& relative) {
    return relative == "font.ttf" || relative == "CLAVIERS.txt" ||
        relative == "fonts" || relative.startsWith("fonts/") ||
        relative == "languages" || relative.startsWith("languages/") ||
        relative == "ime" || relative.startsWith("ime/");
  }
  bool resumeAfter(const String& parent, const String& anchor, File& dir) {
    // Do not hold one open directory per depth: the mounted SD has a finite
    // descriptor pool. Source directories are never removed during merge, so
    // the just-visited directory is a stable cursor when reopening its parent.
    dir = sd.open(parent.c_str(), FILE_READ);
    if (!dir || !dir.isDirectory()) return fail(Error::OpenDirectory);
    const char* wanted = std::strrchr(anchor.c_str(), '/');
    wanted = wanted ? wanted + 1 : anchor.c_str();
    for (;;) {
      errno = 0;
      File child = dir.openNextFile(FILE_READ);
      if (!child) return fail(Error::ReadDirectory);
      const char* name = child.name();
      if (!name) { child.close(); return fail(Error::ReadDirectory); }
      const char* slash = std::strrchr(name, '/');
      if (slash) name = slash + 1;
      const bool found = !std::strcmp(name, wanted);
      child.close();
      if (found) return true;
      if (cooperate) cooperate();
    }
  }
  bool merge(const String& from, const String& to, const String& relative,
             unsigned depth) {
    if (depth > MaxDepth) return fail(Error::DepthLimit);
    File dir = sd.open(from.c_str(), FILE_READ);
    if (!dir || !dir.isDirectory()) return fail(Error::OpenDirectory);
    for (;;) {
      errno = 0;
      File child = dir.openNextFile(FILE_READ);
      const int readError = errno;
      if (!child) {
        dir.close();
        return readError ? fail(Error::ReadDirectory) : true;
      }
      const char* name = child.name();
      if (!name) { child.close(); dir.close(); return fail(Error::ReadDirectory); }
      const char* slash = std::strrchr(name, '/');
      if (slash) name = slash + 1;
      String source, destination, childRelative;
      const bool isDirectory = child.isDirectory();
      if (!name[0] || !std::strcmp(name, ".") || !std::strcmp(name, "..")) {
        child.close(); continue;
      }
      const bool pathsOk = join(from, name, source) && join(to, name, destination) &&
          (relative.length() ? join(relative, name, childRelative)
                             : (childRelative = name, childRelative.length() == std::strlen(name)));
      child.close();
      if (!pathsOk) { dir.close(); return fail(Error::PathTooLong); }
      if (cooperate) cooperate();
      bool present = false;
      if (!exists(destination, present)) { dir.close(); return false; }
      if (!present) {
        // An absent cache sub-tree (possibly thousands of tiles) moves in O(1).
        if (!move(source, destination)) { dir.close(); return false; }
        continue;
      }
      File target = sd.open(destination.c_str(), FILE_READ);
      if (!target) { dir.close(); return fail(Error::OpenDirectory); }
      const bool targetDirectory = target.isDirectory(); target.close();
      if (isDirectory && targetDirectory) {
        dir.close();
        if (!merge(source, destination, childRelative, depth + 1) ||
            !resumeAfter(from, source, dir)) { dir.close(); return false; }
        continue;
      }
      ++result.conflicts;
      if (newResourceWins(childRelative)) continue;  // Old resource remains in legacy backup.
      String backup;
      if (!join(Replaced, childRelative.c_str(), backup) || !parents(backup) ||
          !move(destination, backup) || !move(source, destination)) {
        dir.close(); return false;
      }
      // If power/failure occurs between the two renames above, source remains
      // and destination is absent; a resumed pass installs source directly.
    }
  }
};
}

Result run(fs::FS& nativeSd, void (*cooperate)()) {
  Migration m{nativeSd, cooperate, {}};
  bool oldExists = false, pendingExists = false, newExists = false;
  if (!m.exists(OldRoot, oldExists) || !m.exists(Pending, pendingExists)) return m.result;
  if (!oldExists && !pendingExists) return m.result;
  if (oldExists && !m.directory(OldRoot)) return m.result;
  if (!m.exists(NewRoot, newExists)) return m.result;
  if (!newExists) {
    // Important: do not create System before this O(1) upgrade branch.
    if (pendingExists) { m.fail(Error::NotDirectory); return m.result; }
    if (m.move(OldRoot, NewRoot)) m.result.status = Status::Renamed;
    return m.result;
  }
  if (!m.directory(NewRoot)) return m.result;
  // This reserved subtree contains the journal/backup itself. Refuse an
  // unrelated legacy collision rather than moving a parent into its child.
  bool reservedOld = false, reservedPending = false;
  if (!m.exists("/M5Writer/migration_0824", reservedOld) ||
      !m.exists("/System/migration_0824/pending/legacy/migration_0824", reservedPending)) return m.result;
  if (reservedOld || reservedPending) {
    m.fail(Error::ReservedPath); return m.result;
  }
  if (!m.mkdir(ArchiveRoot) || !m.mkdir(Pending)) return m.result;
  bool legacyExists = false;
  if (!m.exists(Legacy, legacyExists)) return m.result;
  if (!legacyExists) {
    if (!oldExists) { m.fail(Error::NotDirectory); return m.result; }
    if (!m.move(OldRoot, Legacy)) return m.result;
  } else if (oldExists) {
    // A second imported old root must wait until the interrupted one finishes.
    m.fail(Error::BackupCollision); return m.result;
  }
  if (!m.directory(Legacy) || !m.merge(Legacy, NewRoot, "", 0)) return m.result;
  for (unsigned i = 0; i < 1000; ++i) {
    char archive[80];
    std::snprintf(archive, sizeof(archive), "%s/backup-%03u", ArchiveRoot, i);
    bool present = false;
    if (!m.exists(archive, present)) return m.result;
    if (present) continue;
    if (m.move(Pending, archive)) m.result.status = Status::Merged;
    return m.result;
  }
  m.fail(Error::NoBackupName); return m.result;
}

const char* errorName(Error error) {
  switch (error) {
    case Error::None: return "none";
    case Error::NotDirectory: return "not-directory";
    case Error::ReservedPath: return "reserved-path";
    case Error::PathTooLong: return "path-too-long-or-no-memory";
    case Error::DepthLimit: return "depth-limit";
    case Error::OpenDirectory: return "open-failed";
    case Error::ReadDirectory: return "directory-read-failed";
    case Error::Probe: return "path-probe-failed";
    case Error::MakeDirectory: return "mkdir-failed";
    case Error::Rename: return "rename-failed";
    case Error::BackupCollision: return "backup-collision";
    case Error::NoBackupName: return "backup-names-exhausted";
  }
  return "unknown";
}
}
