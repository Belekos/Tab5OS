#include "WriterWorkspace.h"
#include "WriterSyntax.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace writing {
namespace {
constexpr char Primary[] = "/System/editor_workspace.dat";
constexpr char Temporary[] = "/System/editor_workspace.dat.tmp";
constexpr char Backup[] = "/System/editor_workspace.dat.bak";
constexpr char Recovery[] = "/System/editor_workspace.dat.recovery";
constexpr size_t HeaderSize = 20, PrefixSize = 8, ProjectBytes = 365, LegacyEntryBytes = 317, EntryBytes = 318,
                 ExclusionBytes = 9;
constexpr size_t MaxBytes = HeaderSize + PrefixSize + Workspace::MaxProjects * ProjectBytes +
    Workspace::MaxEntries * EntryBytes + Workspace::MaxExclusions * ExclusionBytes;
uint8_t lower(uint8_t c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
bool equalFold(const char* a, const char* b) {
  while (*a && *b) if (lower(uint8_t(*a++)) != lower(uint8_t(*b++))) return false;
  return *a == *b;
}
bool bounded(const char* s, size_t n) { for (size_t i = 0; i < n; ++i) if (!s[i]) return true; return false; }
uint64_t pathHash(const char* p) {
  uint64_t h = UINT64_C(14695981039346656037);
  while (*p) { h ^= lower(uint8_t(*p++)); h *= UINT64_C(1099511628211); }
  return h;
}
uint32_t checksum(uint32_t h, const char* p, size_t n) {
  while (n--) { h ^= uint8_t(*p++); h *= 16777619u; } return h;
}
void put(char*& p, uint64_t v, size_t n) { while (n--) { *p++ = char(v & 255); v >>= 8; } }
uint64_t get(const char*& p, size_t n) {
  uint64_t v = 0; for (size_t i = 0; i < n; ++i) v |= uint64_t(uint8_t(*p++)) << (8 * i); return v;
}
bool validName(const char* p) {
  if (!p || !*p || !bounded(p, 64) || !validUtf8(p, std::strlen(p))) return false;
  for (const char* s = p; *s; ++s) if (uint8_t(*s) < 32 || uint8_t(*s) == 127) return false;
  return true;
}
enum class Integrity { Valid, Invalid, IOFailure };
Integrity intactFile(FileSystem& fs, const char* path) {
  if (!fs.openRead(path)) { fs.close(); return Integrity::IOFailure; }
  const size_t length = fs.length(); char block[512];
  bool ok = length >= HeaderSize + PrefixSize && length <= MaxBytes;
  if (ok && fs.read(block, HeaderSize) != HeaderSize) { fs.close(); return Integrity::IOFailure; }
  const char* cursor = block + 8; uint32_t expected = 0;
  if (ok) {
    const bool old=!std::memcmp(block,"T5WS0001",8), current=!std::memcmp(block,"T5WS0002",8);
    ok=(old||current)&&get(cursor,4)==(old?1u:2u)&&get(cursor,4)==length;
    if (ok) expected = uint32_t(get(cursor, 4));
  }
  uint32_t observed = 2166136261u;
  for (size_t at = HeaderSize; ok && at < length;) {
    size_t n = std::min<size_t>(sizeof(block), length - at);
    if (fs.read(block, n) != n) { fs.close(); return Integrity::IOFailure; }
    observed = checksum(observed, block, n); at += n;
  }
  fs.close(); return ok && observed == expected ? Integrity::Valid : Integrity::Invalid;
}
}

struct Workspace::Data {
  struct Exclusion { uint64_t hash = 0; uint8_t projectID = 0; };
  Entry entries[MaxEntries];
  Project projects[MaxProjects];
  Exclusion exclusions[MaxExclusions];
  uint16_t entriesUsed = 0, exclusionsUsed = 0;
  uint8_t projectsUsed = 0, active = 0;
};
Workspace::~Workspace() { if (data_) { data_->~Data(); release(data_); } }
bool Workspace::begin() {
  if (data_) return true;
  void* memory = allocate(sizeof(Data));
  if (!memory) { error_ = "PSRAM insuffisante pour les projets"; return false; }
  data_ = new (memory) Data(); error_ = ""; return true;
}
size_t Workspace::count() const { return data_ ? data_->entriesUsed : 0; }
Workspace::Entry* Workspace::entry(size_t i) { return i < count() ? &data_->entries[i] : nullptr; }
const Workspace::Entry* Workspace::entry(size_t i) const { return i < count() ? &data_->entries[i] : nullptr; }
size_t Workspace::projectCount() const { return data_ ? data_->projectsUsed : 0; }
Workspace::Project* Workspace::project(size_t i) { return i < projectCount() ? &data_->projects[i] : nullptr; }
const Workspace::Project* Workspace::project(size_t i) const { return i < projectCount() ? &data_->projects[i] : nullptr; }
uint8_t Workspace::activeProject() const { return data_ ? data_->active : 0; }
bool Workspace::validProject(uint8_t id) const {
  if (!id) return true;
  for (size_t i = 0; i < projectCount(); ++i) if (data_->projects[i].id == id) return true;
  return false;
}
bool Workspace::setActiveProject(uint8_t id) {
  if (!data_ || !validProject(id)) return false;
  if (data_->active != id) { data_->active = id; dirty_ = true; }
  return true;
}
bool Workspace::normalizePath(const char* source, char out[PathSize]) {
  if (!source || (source[0] != '/' && source[0] != '\\') || !bounded(source, PathSize)) return false;
  const size_t length = std::strlen(source);
  if (!validUtf8(source, length)) return false;
  size_t used = 1; out[0] = '/'; out[1] = 0;
  for (size_t at = 1; at < length;) {
    while (at < length && (source[at] == '/' || source[at] == '\\')) ++at;
    size_t end = at;
    while (end < length && source[end] != '/' && source[end] != '\\') ++end;
    size_t n = end - at;
    if (!n) break;
    if (n == 1 && source[at] == '.') { at = end; continue; }
    if (n == 2 && source[at] == '.' && source[at + 1] == '.') {
      if (used == 1) return false;
      while (used > 1 && out[used - 1] != '/') --used;
      if (used > 1) --used;
      out[used] = 0; at = end; continue;
    }
    if (source[end - 1] == '.' || source[end - 1] == ' ') return false;
    for (size_t j = at; j < end; ++j) {
      uint8_t c = uint8_t(source[j]);
      if (c < 32 || c == 127 || c == '~' || c == ':' || c == '*' || c == '?' || c == '"' ||
          c == '<' || c == '>' || c == '|') return false;
    }
    if (used + (used > 1 ? 1 : 0) + n >= PathSize) return false;
    if (used > 1) out[used++] = '/';
    std::memcpy(out + used, source + at, n); used += n; out[used] = 0; at = end;
  }
  return true;
}
bool Workspace::samePath(const char* a, const char* b) {
  char ca[PathSize], cb[PathSize];
  return normalizePath(a, ca) && normalizePath(b, cb) && equalFold(ca, cb);
}
int Workspace::find(const char* path, uint8_t id) const {
  char normalized[PathSize];
  if (!normalizePath(path, normalized)) return -1;
  for (size_t i = 0; i < count(); ++i)
    if (data_->entries[i].projectID == id && equalFold(normalized, data_->entries[i].path)) return int(i);
  return -1;
}
size_t Workspace::exclusionCount() const { return data_ ? data_->exclusionsUsed : 0; }
bool Workspace::excluded(const char* path, uint8_t id) const {
  char normalized[PathSize]; if (!data_ || !normalizePath(path, normalized)) return false;
  const uint64_t hash = pathHash(normalized);
  for (size_t i = 0; i < data_->exclusionsUsed; ++i)
    if (data_->exclusions[i].projectID == id && data_->exclusions[i].hash == hash) return true;
  return false;
}
int Workspace::add(const char* path, uint8_t id) { return addImpl(path, id, false); }
int Workspace::addScanned(const char* path, uint8_t id) { return addImpl(path, id, true); }
int Workspace::addImpl(const char* path, uint8_t id, bool scanned) {
  char normalized[PathSize];
  if (!data_ || !validProject(id) || !normalizePath(path, normalized) || normalized[1] == 0) {
    error_ = "Chemin de document invalide"; return -1;
  }
  int existing = find(normalized, id); if (existing >= 0) return existing;
  const uint64_t hash = pathHash(normalized);
  size_t exclusion = data_->exclusionsUsed;
  for (size_t i = 0; i < data_->exclusionsUsed; ++i)
    if (data_->exclusions[i].hash == hash && data_->exclusions[i].projectID == id) { exclusion = i; break; }
  if (scanned && exclusion != data_->exclusionsUsed) { error_ = ""; return -1; }
  if (count() == MaxEntries) { error_ = "Liste limitée à 256 documents"; return -1; }
  if (exclusion < data_->exclusionsUsed) {
    for (size_t i = exclusion + 1; i < data_->exclusionsUsed; ++i) data_->exclusions[i - 1] = data_->exclusions[i];
    --data_->exclusionsUsed;
  }
  Entry& e = data_->entries[data_->entriesUsed]; e = Entry();
  std::strcpy(e.path, normalized); e.projectID = id; dirty_ = true; error_ = "";
  return data_->entriesUsed++;
}
bool Workspace::remove(size_t i) {
  if (i >= count()) return false;
  const Entry& e = data_->entries[i]; bool imported = false;
  // An imported project can later scan any descendant. Removing a manually
  // added member from that project must also keep it out of future scans.
  for (size_t p = 0; p < projectCount(); ++p)
    if (project(p)->id == e.projectID && project(p)->root[0]) imported = true;
  if (imported && !excluded(e.path, e.projectID)) {
    if (data_->exclusionsUsed == MaxExclusions) { error_ = "Liste limitée à 256 exclusions"; return false; }
    auto& x = data_->exclusions[data_->exclusionsUsed++]; x.hash = pathHash(e.path); x.projectID = e.projectID;
  }
  for (size_t j = i + 1; j < count(); ++j) data_->entries[j - 1] = data_->entries[j];
  data_->entries[--data_->entriesUsed] = Entry(); dirty_ = true; error_ = ""; return true;
}
uint8_t Workspace::createProject(const char* name, const char* root) {
  if (!data_ || !validName(name)) { error_ = "Nom de projet invalide"; return 0; }
  if (projectCount() == MaxProjects) { error_ = "Liste limitée à 8 projets"; return 0; }
  char normalized[PathSize] = {};
  if (root && *root && !normalizePath(root, normalized)) { error_ = "Dossier de projet invalide"; return 0; }
  for (size_t i = 0; i < projectCount(); ++i)
    if (equalFold(name, project(i)->name)) { error_ = "Ce projet existe déjà"; return 0; }
  uint8_t id = 1; while (validProject(id)) ++id;
  Project& p = data_->projects[data_->projectsUsed++]; p = Project();
  p.id = id; std::strcpy(p.name, name); std::strcpy(p.root, normalized);
  dirty_ = true; error_ = ""; return id;
}
bool Workspace::valid(const Data& d) const {
  if (d.entriesUsed > MaxEntries || d.projectsUsed > MaxProjects || d.exclusionsUsed > MaxExclusions) return false;
  auto hasProject = [&](uint8_t id) {
    if (!id) return true;
    for (size_t i = 0; i < d.projectsUsed; ++i) if (d.projects[i].id == id) return true;
    return false;
  };
  if (!hasProject(d.active)) return false;
  char normalized[PathSize];
  for (size_t i = 0; i < d.projectsUsed; ++i) {
    const Project& p = d.projects[i];
    if (!p.id || p.id > MaxProjects || !validName(p.name) || !bounded(p.root, PathSize)) return false;
    if (p.root[0] && (!normalizePath(p.root, normalized) || std::strcmp(p.root, normalized))) return false;
    for (size_t j = 0; j < i; ++j)
      if (p.id == d.projects[j].id || equalFold(p.name, d.projects[j].name)) return false;
  }
  for (size_t i = 0; i < d.entriesUsed; ++i) {
    const Entry& e = d.entries[i];
    if (!bounded(e.path, PathSize) || !normalizePath(e.path, normalized) || normalized[1] == 0 ||
        std::strcmp(e.path, normalized) || !hasProject(e.projectID) || e.start > Document::Limit ||
        e.cursor > Document::Limit || e.anchor > Document::Limit || e.scroll < 0 || e.syntaxLanguage >= uint8_t(SyntaxLanguage::Count)) return false;
    for (size_t j = 0; j < i; ++j)
      if (e.projectID == d.entries[j].projectID && equalFold(e.path, d.entries[j].path)) return false;
  }
  for (size_t i = 0; i < d.exclusionsUsed; ++i) {
    const auto& x = d.exclusions[i]; if (!x.projectID || !hasProject(x.projectID)) return false;
    for (size_t j = 0; j < i; ++j)
      if (x.projectID == d.exclusions[j].projectID && x.hash == d.exclusions[j].hash) return false;
    for (size_t j = 0; j < d.entriesUsed; ++j)
      if (x.projectID == d.entries[j].projectID && x.hash == pathHash(d.entries[j].path)) return false;
  }
  return true;
}
bool Workspace::readFile(FileSystem& fs, const char* path, Data& d) {
  readIO_ = false;
  bool present=false;if(!fs.probe(path,present)){readIO_=true;return false;}
  if(!present)return false;
  if (!fs.openRead(path)) { fs.close(); readIO_ = true; return false; }
  const size_t length = fs.length(); char block[512];
  bool ok = length >= HeaderSize + PrefixSize && length <= MaxBytes;
  if (ok && fs.read(block, HeaderSize) != HeaderSize) { ok = false; readIO_ = true; }
  const char* cursor = block + 8;
  uint32_t expected = 0;size_t entryBytes=LegacyEntryBytes;
  if (ok) {
    const bool old=!std::memcmp(block,"T5WS0001",8), current=!std::memcmp(block,"T5WS0002",8);
    ok=(old||current)&&get(cursor,4)==(old?1u:2u)&&get(cursor,4)==length;
    entryBytes=current?EntryBytes:LegacyEntryBytes;
    expected = uint32_t(get(cursor, 4));
  }
  uint32_t observed = 2166136261u;
  auto read = [&](size_t n) {
    if (!ok || n > sizeof(block)) { ok = false; return false; }
    if (fs.read(block, n) != n) { ok = false; readIO_ = true; return false; }
    observed = checksum(observed, block, n); return true;
  };
  if (read(PrefixSize)) {
    cursor = block; d.entriesUsed = uint16_t(get(cursor, 2)); d.projectsUsed = uint8_t(get(cursor, 1));
    d.active = uint8_t(get(cursor, 1)); d.exclusionsUsed = uint16_t(get(cursor, 2));
    ok = get(cursor, 2) == 0 && d.entriesUsed <= MaxEntries && d.projectsUsed <= MaxProjects &&
      d.exclusionsUsed <= MaxExclusions && length == HeaderSize + PrefixSize +
      d.entriesUsed * entryBytes + d.projectsUsed * ProjectBytes + d.exclusionsUsed * ExclusionBytes;
  }
  for (size_t i = 0; ok && i < d.projectsUsed; ++i) if (read(ProjectBytes)) {
    auto& p = d.projects[i]; std::memcpy(p.name, block, 64); std::memcpy(p.root, block + 64, PathSize);
    p.id = uint8_t(block[364]);
  }
  for (size_t i = 0; ok && i < d.entriesUsed; ++i) if (read(entryBytes)) {
    auto& e = d.entries[i]; std::memcpy(e.path, block, PathSize); cursor = block + PathSize;
    e.projectID = uint8_t(get(cursor, 1)); e.start = uint32_t(get(cursor, 4));
    e.cursor = uint32_t(get(cursor, 4)); e.anchor = uint32_t(get(cursor, 4));
    e.scroll = int32_t(uint32_t(get(cursor, 4)));
    e.syntaxLanguage=entryBytes==EntryBytes?uint8_t(get(cursor,1)):0;
  }
  for (size_t i = 0; ok && i < d.exclusionsUsed; ++i) if (read(ExclusionBytes)) {
    cursor = block; d.exclusions[i].hash = get(cursor, 8); d.exclusions[i].projectID = uint8_t(get(cursor, 1));
  }
  fs.close(); return ok && observed == expected && valid(d);
}
bool Workspace::load(FileSystem& fs) {
  if (busy_ || !begin()) return false;
  busy_ = true;
  Data* next = static_cast<Data*>(allocate(sizeof(Data)));
  if (!next) { busy_ = false; error_ = "PSRAM insuffisante pour les projets"; return false; }
  new (next) Data();
  bool loaded = readFile(fs, Primary, *next), recovered = false;
  primaryInvalid_ = !loaded; primaryIO_ = readIO_; temporaryRecovered_ = false;
  if (!loaded) { loaded = readFile(fs, Backup, *next); primaryIO_|=readIO_; recovered = loaded; }
  if (!loaded) { loaded = readFile(fs, Recovery, *next); primaryIO_|=readIO_; recovered = loaded; }
  if (!loaded) { loaded = readFile(fs, Temporary, *next); primaryIO_|=readIO_; recovered = loaded; temporaryRecovered_ = loaded; }
  if (loaded) { Data* old = data_; data_ = next; next = old; dirty_ = recovered; error_ = ""; }
  else {
    bool absent=!primaryIO_;
    for(const char* path:{Primary,Backup,Recovery,Temporary}){
      bool present=false;if(!fs.probe(path,present)){primaryIO_=true;absent=false;}else if(present)absent=false;
    }
    if(absent){
      // First start is valid only when every store is known to be absent.
      Data* old = data_; data_ = next; next = old; dirty_ = false; error_ = ""; loaded = true;
    }else error_ = "Liste des documents illisible";
  }
  next->~Data(); release(next); busy_ = false; return loaded;
}
bool Workspace::save(FileSystem& fs, void (*yieldFn)()) {
  if (busy_ || !data_) return false;
  if (!dirty_) return true;
  if (primaryIO_) { error_ = "Lecture SD incomplète ; projets conservés"; return false; }
  if (!valid(*data_)) { error_ = "Liste des documents invalide"; return false; }
  const size_t length = HeaderSize + PrefixSize + count() * EntryBytes + projectCount() * ProjectBytes +
      data_->exclusionsUsed * ExclusionBytes;
  char* bytes = static_cast<char*>(allocate(length));
  if (!bytes) { error_ = "PSRAM insuffisante pour les projets"; return false; }
  struct BufferGuard { char* bytes; ~BufferGuard() { release(bytes); } } bufferGuard{bytes};
  busy_ = true; std::memset(bytes, 0, length); std::memcpy(bytes, "T5WS0002", 8);
  char* p = bytes + 8; put(p, 2, 4); put(p, length, 4); put(p, 0, 4);
  put(p, count(), 2); put(p, projectCount(), 1); put(p, activeProject(), 1); put(p, data_->exclusionsUsed, 2); put(p, 0, 2);
  for (size_t i = 0; i < projectCount(); ++i) {
    const Project& x = *project(i); std::memcpy(p, x.name, 64); p += 64;
    std::memcpy(p, x.root, PathSize); p += PathSize; put(p, x.id, 1);
  }
  for (size_t i = 0; i < count(); ++i) {
    const Entry& x = *entry(i); std::memcpy(p, x.path, PathSize); p += PathSize; put(p, x.projectID, 1);
    put(p, x.start, 4); put(p, x.cursor, 4); put(p, x.anchor, 4); put(p, uint32_t(x.scroll), 4); put(p,x.syntaxLanguage,1);
  }
  for (size_t i = 0; i < data_->exclusionsUsed; ++i) {
    put(p, data_->exclusions[i].hash, 8); put(p, data_->exclusions[i].projectID, 1);
  }
  p = bytes + 16; put(p, checksum(2166136261u, bytes + HeaderSize, length - HeaderSize), 4);
  auto finish = [&](bool success, const char* error) {
    fs.close(); busy_ = false; error_ = error;
    if (success) { dirty_ = false; primaryInvalid_ = false; primaryIO_ = false; } return success;
  };
  // A first-save power cut may leave only a validated temporary. Preserve that
  // recovered version before reusing the temporary path for the next attempt.
  if (temporaryRecovered_) {
    if (intactFile(fs, Temporary) != Integrity::Valid) return finish(false, "Lecture SD incomplète ; projets conservés");
    bool present=false;if (!fs.probe(Recovery,present)||(present&&!fs.remove(Recovery))) return finish(false, "Copie précédente inaccessible");
    if (!fs.rename(Temporary, Recovery)) return finish(false, "Impossible de préserver les projets");
    temporaryRecovered_ = false;
  }
  bool present=false;if (!fs.probe(Temporary,present)||(present&&!fs.remove(Temporary))) return finish(false, "Fichier temporaire inaccessible");
  if (!fs.openWrite(Temporary)) return finish(false, "Impossible de sauvegarder les projets");
  for (size_t at = 0; at < length;) {
    size_t n = std::min<size_t>(4096, length - at);
    if (fs.write(bytes + at, n) != n) return finish(false, "Écriture SD incomplète");
    at += n; if (yieldFn) yieldFn();
  }
  if(!fs.finishWrite())return finish(false,"Écriture SD incomplète");
  if (!fs.openRead(Temporary) || fs.length() != length) return finish(false, "Vérification SD échouée");
  char block[512];
  for (size_t at = 0; at < length;) {
    const size_t n = std::min<size_t>(sizeof(block), length - at);
    if (fs.read(block, n) != n || std::memcmp(block, bytes + at, n)) return finish(false, "Vérification SD échouée");
    at += n; if (yieldFn) yieldFn();
  }
  fs.close();
  bool original=false;if(!fs.probe(Primary,original))return finish(false, "Lecture SD incomplète ; projets conservés");
  // A later SD corruption must not overwrite the last valid backup.
  const Integrity currentIntegrity = original && !primaryInvalid_ ? intactFile(fs, Primary) : Integrity::Invalid;
  if (currentIntegrity == Integrity::IOFailure) return finish(false, "Lecture SD incomplète ; projets conservés");
  const bool preserveOriginal = original && !primaryInvalid_ && currentIntegrity == Integrity::Valid;
  bool moved = false;
  if (preserveOriginal) {
    if (!fs.probe(Backup,present)||(present&&!fs.remove(Backup))) return finish(false, "Copie précédente inaccessible");
    if (!fs.rename(Primary, Backup)) return finish(false, "Impossible de préserver les projets");
    moved = true;
  } else if (original && !fs.remove(Primary)) return finish(false, "Impossible de préserver les projets");
  if (!fs.rename(Temporary, Primary)) {
    if (moved) fs.rename(Backup, Primary);
    return finish(false, "Validation des projets impossible");
  }
  return finish(true, "");
}
} // namespace writing
