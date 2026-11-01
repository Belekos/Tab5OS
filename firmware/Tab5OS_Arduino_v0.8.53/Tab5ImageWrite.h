#pragma once
#include <stddef.h>
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <cerrno>

// Streaming ABI-6 output. Testable with a filesystem double; the same class is
// instantiated over Tab5VolumeFS in firmware. Only the worker calls it, then the
// OS aborts once that worker has stopped. No retained application buffers.
template<class Fs, class File, bool AllowJpeg = false> class Tab5ImageWrite {
 public:
  static constexpr size_t MaxBytes = 16u * 1024u * 1024u;
  static bool valid(const char* path) {
    if (!path) return false;
    const size_t prefix = !strncmp(path,"/Picture/",9) ? 9 : !strncmp(path,"/Images/",8) ? 8 : 0;
    if (!prefix) return false;
    const size_t n = strnlen(path, 261);
    if (n < prefix+5 || n > 260 || path[prefix] == '.') return false;
    const char* ext = strrchr(path + prefix, '.');
    if (!ext) return false;
    const size_t extLength = strlen(ext);
    bool accepted = false;
    if (extLength == 4) {
      const char a = ext[1] | 32, b = ext[2] | 32, c = ext[3] | 32;
      accepted = (a == 'b' && b == 'm' && c == 'p') ||
                 (a == 't' && ext[2] == '5' && c == 'i') ||
                 (AllowJpeg && a == 'j' && b == 'p' && c == 'g');
    } else if (AllowJpeg && extLength == 5) {
      accepted = (ext[1] | 32) == 'j' && (ext[2] | 32) == 'p' &&
                 (ext[3] | 32) == 'e' && (ext[4] | 32) == 'g';
    }
    if (!accepted) return false;
    for (size_t i = prefix; i < n; ++i) {
      unsigned char c = path[i];
      if (c < 32 || c == 127 || strchr("/\\:*?\"<>|~", c)) return false;
    }
    return true;
  }
  static bool recover(Fs& fs, const char* path) {
    if (!valid(path)) return false;
    char backup[273]; snprintf(backup, sizeof(backup), "%s.tab5old", path);
    bool present = false;
    if (!probe(fs, path, present)) return false;
    if (present) return true;
    if (!probe(fs, backup, present)) return false;
    return !present || fs.rename(backup, path);
  }
  int begin(Fs& fs, const char* path, const char* mode) {
    if (active_ || !valid(path)) return -1;
    const char* directory = !strncmp(path,"/Picture/",9) ? "/Picture" : "/Images";
    bool present = false;
    if (!probe(fs, directory, present) || (!present && !fs.mkdir(directory))) return -1;
    snprintf(path_, sizeof(path_), "%s", path);
    snprintf(temporary_, sizeof(temporary_), "%s.tab5tmp", path);
    snprintf(backup_, sizeof(backup_), "%s.tab5old", path);
    if (!recover(fs, path_)) return -1;
    if (!probe(fs, temporary_, present) || (present && !fs.remove(temporary_))) return -1;
    file_ = fs.open(temporary_, mode);
    if (!file_) return -1;
    bytes_ = 0; failed_ = false; active_ = true;
    serial_ = serial_ >= 0x7fffffffU ? 1 : serial_ + 1;
    return int(serial_);
  }
  int chunk(int handle, const void* data, size_t size) {
    if (!active_ || handle != int(serial_) || failed_) return -1;
    if (!data || !size || size > 4096 || size > MaxBytes - bytes_) { failed_ = true; return -1; }
    size_t done = file_.write(static_cast<const uint8_t*>(data), size);
    bytes_ += done;
    if (done != size || file_.getWriteError()) { failed_ = true; return -1; }
    return int(done);
  }
  int finish(Fs& fs, int handle, bool commit) {
    if (!active_ || handle != int(serial_)) return -1;
    bool synced = false;
    if (commit) { errno = 0; file_.flush(); synced = !errno; }
    // Arduino flush() exposes fflush/fsync failures through errno, without
    // setting Stream's write-error flag. Capture it before another FS call.
    bool ok = commit && synced && !failed_ && bytes_ && !file_.getWriteError() && file_.size() == bytes_;
    errno = 0; file_.close(); ok = ok && !errno; active_ = false;
    if (!ok) { fs.remove(temporary_); return commit ? -1 : 0; }
    bool original = false, present = false;
    if (!probe(fs, path_, original)) return -1;
    if (original) {
      if (!probe(fs, backup_, present) || (present && !fs.remove(backup_))) { fs.remove(temporary_); return -1; }
      if (!fs.rename(path_, backup_)) { fs.remove(temporary_); return -1; }
    }
    if (!fs.rename(temporary_, path_)) {
      if (original) fs.rename(backup_, path_);
      fs.remove(temporary_); return -1;
    }
    fs.remove(backup_); return 0;
  }
  void abort(Fs& fs) { if (active_) finish(fs, int(serial_), false); }
 private:
  // Production uses the SDK FS adapter, which preserves ENOENT versus I/O
  // errors. Filesystem doubles must preserve that same exists()/errno contract.
  static bool probe(Fs& fs, const char* path, bool& present) {
    errno = 0; present = fs.exists(path); return present || errno == ENOENT;
  }
  File file_;
  char path_[261]{}, temporary_[273]{}, backup_[273]{};
  size_t bytes_ = 0;
  uint32_t serial_ = 0;
  bool active_ = false, failed_ = false;
};
