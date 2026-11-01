#include "Tab5FileOperation.h"
#include "Tab5FilePolicy.h"
#include "Tab5Storage.h"
#include "Tab5Task.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <new>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

namespace Tab5FileOperation {
namespace {
constexpr unsigned MaxEntries = 1024, MaxDepth = 16, BufferSize = 8192;
constexpr unsigned TaskStack = 12 * 1024;
enum State : uint8_t { Idle, Running, Complete };
std::atomic<uint8_t> state{Idle};
std::atomic<bool> stop{false};
portMUX_TYPE progressLock = portMUX_INITIALIZER_UNLOCKED;
Snapshot progress;
Completion completion;
struct Entry {
  char relative[PathCapacity];
  uint64_t size;
  time_t modified;
  uint16_t parent;
  uint8_t depth;
  bool directory;
};
struct Job {
  // Raw POSIX paths do not hold Tab5VolumeFS references. This lease deliberately
  // blocks both SD recovery and safe USB eject, even between individual files.
  Tab5StorageUse storage;
  Entry* entries = nullptr;
  uint8_t* buffers = nullptr;
  unsigned count = 0;
  int error = 0;
  Completion result;
  ~Job() { free(entries); free(buffers); }
};
struct Descriptor {
  int fd = -1;
  ~Descriptor() { if (fd >= 0) close(fd); }
  bool finish() { int n = fd; fd = -1; return n < 0 || close(n) == 0; }
};
struct Directory {
  DIR* dir = nullptr;
  ~Directory() { if (dir) closedir(dir); }
};
void publish(Phase phase, uint32_t files, uint64_t bytes, int error = 0) {
  portENTER_CRITICAL(&progressLock);
  progress.phase = phase; progress.filesDone = files;
  progress.bytesDone = bytes; progress.error = error;
  portEXIT_CRITICAL(&progressLock);
}
bool fail(Job& job, int error, const char* path = nullptr) {
  if (!job.error) job.error = error ? error : EIO;
  if (path && (job.error == EIO || job.error == ENODEV || job.error == ENXIO ||
      job.error == ETIMEDOUT || job.error == ENOSPC || job.error == EROFS))
    tab5StorageFault("Copie de fichiers", path, job.error);
  return false;
}
bool continuing(Job& job) {
  if (stop.load(std::memory_order_relaxed)) return fail(job, ECANCELED);
  return true;
}
bool logicalPath(const char* path) {
  if (!path || !*path || strlen(path) >= PathCapacity || path[0] != '/') return false;
  std::string canonical;
  if (!Tab5Files::canonical(path, canonical) || canonical == "/" || canonical == "/usb") return false;
  // Do not accept normalized aliases: joining children must never introduce '..'.
  if (strchr(path, '\\') || strstr(path, "//")) return false;
  const char* part = path + 1;
  while (*part) {
    const char* slash = strchr(part, '/');
    size_t length = slash ? size_t(slash - part) : strlen(part);
    if (!length || (length == 1 && part[0] == '.') ||
        (length == 2 && part[0] == '.' && part[1] == '.')) return false;
    if (!slash) break;
    part = slash + 1;
    if (!*part) return false;
  }
  return true;
}
bool diskPath(Job& job, const char* root, const char* relative, char* out) {
  const int n = snprintf(out, PathCapacity + 4, "/sd%s%s%s", root,
                         *relative ? "/" : "", relative);
  if (n < 0 || n >= int(PathCapacity + 3)) return fail(job, ENAMETOOLONG);
  return true;
}
bool same(const struct stat& info, const Entry& entry) {
  return (entry.directory ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode)) &&
      (entry.directory || (info.st_size >= 0 && uint64_t(info.st_size) == entry.size &&
                           info.st_mtime == entry.modified));
}
bool inspect(Job& job, Entry& entry, const char* source) {
  char path[PathCapacity + 4]; struct stat info{};
  if (!diskPath(job, source, entry.relative, path)) return false;
  if (stat(path, &info)) return fail(job, errno, path);
  if (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode)) return fail(job, EINVAL);
  if (info.st_size < 0) return fail(job, EFBIG);
  entry.directory = S_ISDIR(info.st_mode);
  entry.size = entry.directory ? 0 : uint64_t(info.st_size);
  entry.modified = info.st_mtime;
  return true;
}
bool scan(Job& job) {
  job.count = 1;
  if (!inspect(job, job.entries[0], job.result.source)) return false;
  job.result.sourceIsDirectory = job.entries[0].directory;
  uint64_t total = job.entries[0].size;
  unsigned files = job.entries[0].directory ? 0 : 1;
  // Breadth-first manifest: one directory handle, no recursive stack or growing
  // vectors. The manifest also detects added/removed directory entries.
  for (unsigned i = 0; i < job.count; ++i) {
    if (!continuing(job)) return false;
    Entry& parent = job.entries[i];
    if (!parent.directory) continue;
    char path[PathCapacity + 4];
    if (!diskPath(job, job.result.source, parent.relative, path)) return false;
    Directory directory; directory.dir = opendir(path);
    if (!directory.dir) return fail(job, errno, path);
    while (true) {
      if (!continuing(job)) return false;
      errno = 0; struct dirent* item = readdir(directory.dir);
      if (!item) { if (errno) return fail(job, errno, path); break; }
      if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
      if (job.count == MaxEntries || parent.depth == MaxDepth) return fail(job, E2BIG);
      Entry& entry = job.entries[job.count];
      int n = snprintf(entry.relative, sizeof(entry.relative), "%s%s%s", parent.relative,
                       *parent.relative ? "/" : "", item->d_name);
      if (n < 0 || n >= int(sizeof(entry.relative))) return fail(job, ENAMETOOLONG);
      // Reserve enough room for BOTH complete paths before creating anything.
      if (strlen(job.result.source) + 1 + size_t(n) >= PathCapacity ||
          strlen(job.result.destination) + 1 + size_t(n) >= PathCapacity)
        return fail(job, ENAMETOOLONG);
      char logical[PathCapacity];
      snprintf(logical, sizeof(logical), "%s/%s", job.result.source, entry.relative);
      if (!logicalPath(logical)) return fail(job, EINVAL);
      entry.depth = parent.depth + 1; entry.parent = i;
      if (!inspect(job, entry, job.result.source)) return false;
      if (!entry.directory) { total += entry.size; ++files; }
      ++job.count;
      vTaskDelay(1);
    }
  }
  portENTER_CRITICAL(&progressLock);
  progress.filesTotal = files; progress.bytesTotal = total;
  portEXIT_CRITICAL(&progressLock);
  return true;
}
ssize_t readSome(int fd, uint8_t* data, size_t length) {
  ssize_t n;
  do { n = read(fd, data, length); } while (n < 0 && errno == EINTR);
  return n;
}
bool writeAll(Job& job, int fd, const uint8_t* data, size_t length, const char* path) {
  while (length) {
    if (!continuing(job)) return false;
    ssize_t n = write(fd, data, length);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return fail(job, n < 0 ? errno : EIO, path);
    data += n; length -= size_t(n);
  }
  return true;
}
bool copyFile(Job& job, const Entry& entry, const char* source, const char* destination,
              unsigned filesDone, uint64_t& bytesDone) {
  Descriptor input, output; struct stat info{};
  input.fd = open(source, O_RDONLY);
  if (input.fd < 0) return fail(job, errno, source);
  if (fstat(input.fd, &info)) return fail(job, errno, source);
  if (!same(info, entry)) return fail(job, ESTALE);
  // O_EXCL is intentional: neither a concurrent creator nor a preexisting file
  // can be truncated, even if the UI's unique-name choice became stale.
  output.fd = open(destination, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (output.fd < 0) return fail(job, errno, destination);
  job.result.destinationPresent = true;
  uint64_t remaining = entry.size;
  while (remaining) {
    if (!continuing(job)) return false;
    size_t request = size_t(std::min<uint64_t>(remaining, BufferSize));
    ssize_t got = readSome(input.fd, job.buffers, request);
    if (got <= 0) return fail(job, got < 0 ? errno : EIO, source);
    if (!writeAll(job, output.fd, job.buffers, size_t(got), destination)) return false;
    remaining -= uint64_t(got); bytesDone += uint64_t(got);
    publish(Phase::Copying, filesDone, bytesDone);
    vTaskDelay(1);
  }
  ssize_t extra = readSome(input.fd, job.buffers, 1);
  if (extra < 0) return fail(job, errno, source);
  if (extra != 0) return fail(job, ESTALE);
  if (fstat(input.fd, &info)) return fail(job, errno, source);
  if (!same(info, entry)) return fail(job, ESTALE);
  if (fsync(output.fd)) return fail(job, errno, destination);
  if (fstat(output.fd, &info)) return fail(job, errno, destination);
  if (info.st_size < 0 || uint64_t(info.st_size) != entry.size) return fail(job, EIO, destination);
  if (!output.finish()) return fail(job, errno, destination);
  if (!input.finish()) return fail(job, errno, source);
  return true;
}
bool compareFile(Job& job, const Entry& entry, const char* source, const char* destination) {
  Descriptor input, output; struct stat a{}, b{};
  input.fd = open(source, O_RDONLY);
  if (input.fd < 0) return fail(job, errno, source);
  output.fd = open(destination, O_RDONLY);
  if (output.fd < 0) return fail(job, errno, destination);
  if (fstat(input.fd, &a) || fstat(output.fd, &b)) return fail(job, errno);
  if (!same(a, entry) || !S_ISREG(b.st_mode) || b.st_size < 0 || uint64_t(b.st_size) != entry.size)
    return fail(job, ESTALE);
  uint64_t remaining = entry.size;
  while (remaining) {
    if (!continuing(job)) return false;
    size_t length = size_t(std::min<uint64_t>(remaining, BufferSize));
    size_t gotA = 0, gotB = 0;
    while (gotA < length) {
      ssize_t n = readSome(input.fd, job.buffers + gotA, length - gotA);
      if (n <= 0) return fail(job, n < 0 ? errno : EIO, source);
      gotA += size_t(n);
    }
    while (gotB < length) {
      ssize_t n = readSome(output.fd, job.buffers + BufferSize + gotB, length - gotB);
      if (n <= 0) return fail(job, n < 0 ? errno : EIO, destination);
      gotB += size_t(n);
    }
    if (memcmp(job.buffers, job.buffers + BufferSize, length)) return fail(job, EIO, destination);
    remaining -= length;
    vTaskDelay(1);
  }
  // Recheck after reads: changes made while copying/verification retain originals.
  if (fstat(input.fd, &a) || fstat(output.fd, &b)) return fail(job, errno);
  if (!same(a, entry) || b.st_size < 0 || uint64_t(b.st_size) != entry.size) return fail(job, ESTALE);
  if (!input.finish() || !output.finish()) return fail(job, errno);
  return true;
}
bool directoryUnchanged(Job& job, unsigned index, const char* source) {
  Directory dir; dir.dir = opendir(source);
  if (!dir.dir) return fail(job, errno, source);
  unsigned children = 0, expected = 0;
  for (unsigned i = 1; i < job.count; ++i) if (job.entries[i].parent == index) ++expected;
  while (true) {
    if (!continuing(job)) return false;
    errno = 0; struct dirent* item = readdir(dir.dir);
    if (!item) { if (errno) return fail(job, errno, source); break; }
    if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
    bool found = false;
    for (unsigned i = 1; i < job.count; ++i) {
      if (job.entries[i].parent != index) continue;
      const char* name = strrchr(job.entries[i].relative, '/');
      name = name ? name + 1 : job.entries[i].relative;
      if (!strcmp(name, item->d_name)) { found = true; break; }
    }
    if (!found || ++children > expected) return fail(job, ESTALE);
    vTaskDelay(1);
  }
  return children == expected || fail(job, ESTALE);
}
bool execute(Job& job) {
  char source[PathCapacity + 4], destination[PathCapacity + 4];
  struct stat info{};
  if (!diskPath(job, job.result.destination, "", destination)) return false;
  if (!stat(destination, &info)) return fail(job, EEXIST);
  if (errno != ENOENT) return fail(job, errno, destination);
  publish(Phase::Scanning, 0, 0);
  if (!scan(job)) return false;
  unsigned files = 0; uint64_t bytes = 0;
  for (unsigned i = 0; i < job.count; ++i) {
    if (!continuing(job)) return false;
    Entry& entry = job.entries[i];
    if (!diskPath(job, job.result.source, entry.relative, source) ||
        !diskPath(job, job.result.destination, entry.relative, destination)) return false;
    if (entry.directory) {
      if (mkdir(destination, 0777)) return fail(job, errno, destination);
      job.result.destinationPresent = true;
    } else {
      if (!copyFile(job, entry, source, destination, files, bytes)) return false;
      ++files;
    }
    publish(Phase::Copying, files, bytes);
    vTaskDelay(1);
  }
  publish(Phase::Verifying, 0, bytes);
  files = 0;
  for (unsigned i = 0; i < job.count; ++i) {
    if (!continuing(job)) return false;
    Entry& entry = job.entries[i];
    if (!diskPath(job, job.result.source, entry.relative, source) ||
        !diskPath(job, job.result.destination, entry.relative, destination)) return false;
    if (entry.directory) {
      if (!directoryUnchanged(job, i, source)) return false;
    } else {
      if (!compareFile(job, entry, source, destination)) return false;
      publish(Phase::Verifying, ++files, bytes);
    }
  }
  job.result.success = true;
  // No compare/unlink pair can exclude an editor, Python script or WebDAV write
  // in between without a filesystem-wide mutation lock. Fallback moves therefore
  // keep every original; the UI reports a verified copy with source retained.
  // The UI may use its short native same-volume rename before starting this job.
  return true;
}
void worker(void* argument) {
  auto* job = static_cast<Job*>(argument);
  execute(*job);
  job->result.error = job->error;
  job->result.cancelled = job->error == ECANCELED;
  completion = job->result;
  Snapshot last; snapshot(last);
  publish(completion.cancelled ? Phase::Cancelled :
      completion.error ? Phase::Failed : Phase::Done,
      last.filesDone, last.bytesDone, completion.error);
  delete job; // Close the whole-operation storage lease BEFORE publishing idle.
  tab5TaskCheckpoint("Copie fichiers");
  state.store(Complete, std::memory_order_release);
  vTaskDelete(nullptr);
}
bool reject(int error) {
  publish(Phase::Failed, 0, 0, error);
  state.store(Idle, std::memory_order_release);
  return false;
}
} // namespace
bool startCopy(const char* source, const char* destination, bool move) {
  uint8_t idle = Idle;
  if (!state.compare_exchange_strong(idle, Running, std::memory_order_acq_rel)) return false;
  if (!logicalPath(source) || !logicalPath(destination) ||
      Tab5Files::sameOrDescendant(destination, source)) return reject(EINVAL);
  stop.store(false, std::memory_order_relaxed);
  portENTER_CRITICAL(&progressLock); progress = {}; portEXIT_CRITICAL(&progressLock);
  const size_t working = sizeof(Entry) * MaxEntries + 2 * BufferSize;
  Tab5Resources::Budget budget{TaskStack + 8192, TaskStack, 0, working, sizeof(Entry) * MaxEntries};
  auto admission = tab5ReserveMemory(Tab5Resources::Owner::Files, budget, "Copie de fichiers");
  if (!admission) return reject(ENOMEM);
  auto* job = new(std::nothrow) Job;
  if (!job) return reject(ENOMEM);
  if (!job->storage) { delete job; return reject(EBUSY); }
  job->entries = static_cast<Entry*>(heap_caps_calloc(MaxEntries, sizeof(Entry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  job->buffers = static_cast<uint8_t*>(heap_caps_malloc(2 * BufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!job->entries || !job->buffers) { delete job; return reject(ENOMEM); }
  admission.allocated(sizeof(Job), working);
  strcpy(job->result.source, source); strcpy(job->result.destination, destination);
  job->result.requestedMove = move;
  publish(Phase::Scanning, 0, 0);
  if (tab5CreateAdmittedTask(admission, worker, "tab5-file-copy", TaskStack,
                            job, 1, nullptr, 0) != pdPASS) {
    delete job; return reject(ENOMEM);
  }
  return true;
}
bool busy() { return state.load(std::memory_order_acquire) == Running; }
void cancel() { if (busy()) stop.store(true, std::memory_order_relaxed); }
void snapshot(Snapshot& out) {
  portENTER_CRITICAL(&progressLock); out = progress; portEXIT_CRITICAL(&progressLock);
}
const char* status() {
  Snapshot value; snapshot(value);
  switch (value.phase) {
    case Phase::Idle: return "Prêt";
    case Phase::Scanning: return "Préparation de la copie…";
    case Phase::Copying: return "Copie en cours…";
    case Phase::Verifying: return "Vérification de la copie…";
    case Phase::Removing: return "Finalisation du déplacement…";
    case Phase::Done: return "Opération terminée";
    case Phase::Cancelled: return "Opération annulée";
    case Phase::Failed: break;
  }
  switch (value.error) {
    case EEXIST: return "Destination déjà présente : aucun écrasement";
    case ENOMEM: return "Mémoire insuffisante pour la copie";
    case EBUSY: return "Stockage occupé";
    case E2BIG: return "Copiez par sous-dossiers : limite de 1024 éléments ou 16 niveaux";
    case ENAMETOOLONG: return "Chemin trop long pour la copie";
    case ENOSPC: return "Espace insuffisant sur la destination";
    case ESTALE: return "La source a changé : original conservé";
    case EINVAL: return "Chemin invalide pour la copie";
    case ENOENT: return "Source ou dossier de destination introuvable";
    default: return "Erreur de copie : vérifier la source et la destination";
  }
}
bool takeCompletion(Completion& out) {
  if (state.load(std::memory_order_acquire) != Complete) return false;
  out = completion;
  state.store(Idle, std::memory_order_release);
  return true;
}
} // namespace Tab5FileOperation
