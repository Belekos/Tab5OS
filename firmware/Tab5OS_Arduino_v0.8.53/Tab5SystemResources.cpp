#include "Tab5SystemResources.h"
#include "Tab5SystemResourcesData.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5ResourceManager.h"
#include "src/tab5git/deps/zlib/zlib.h"
#include <esp_heap_caps.h>
#include <Arduino.h>
#include <FS.h>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <new>
#include <utility>

namespace Tab5SystemResources { namespace {
enum class Phase { Idle, Select, Decode, Write, Verify, Commit };
Phase phase = Phase::Idle;
size_t index = 0, restored = 0, position = 0;
File file;
String temporary;
Tab5StorageUse* lease = nullptr;
const char* message = "Ressources système disponibles";

// zlib's inflate state and 32 KiB history window are allocated on demand in
// PSRAM. The workspace cap bounds an unexpected allocator request as well as
// admission. No decompression/allocation runs at boot or while repair is idle.
constexpr size_t MaxResource = 128 * 1024;
constexpr size_t InflateWorkspace = 48 * 1024;
struct Decoder {
  z_stream stream{};
  Tab5Resources::Reservation reservation;
  uint8_t* bytes = nullptr;
  size_t allocated = 0;
  bool initialized = false;
};
Decoder* decoder = nullptr;
voidpf allocateInflate(voidpf opaque, uInt count, uInt size) {
  Decoder* state = static_cast<Decoder*>(opaque);
  if (size && count > InflateWorkspace / size) return nullptr;
  const size_t bytes = size_t(count) * size;
  if (bytes > InflateWorkspace - state->allocated) return nullptr;
  void* memory = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (memory) { state->allocated += bytes; state->reservation.allocated(0, bytes); }
  return memory;
}
void freeInflate(voidpf, voidpf memory) { heap_caps_free(memory); }
void releaseDecoder() {
  if (!decoder) return;
  if (decoder->initialized) inflateEnd(&decoder->stream);
  heap_caps_free(decoder->bytes);
  decoder->~Decoder(); heap_caps_free(decoder); decoder = nullptr;
}
bool beginDecode(const Tab5SystemResource& entry) {
  const size_t output = entry.size + 1; // one sentinel byte detects excess output
  Tab5Resources::Budget budget{};
  budget.psram = sizeof(Decoder) + output + InflateWorkspace;
  budget.psramBlock = std::max(output, size_t(32768));
  auto reservation = tab5ReserveMemory(Tab5Resources::Owner::System, budget, "resource repair");
  if (!reservation) return false;
  void* memory = heap_caps_malloc(sizeof(Decoder), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!memory) return false;
  decoder = new(memory) Decoder;
  decoder->reservation = std::move(reservation);
  decoder->reservation.allocated(0, sizeof(Decoder));
  decoder->bytes = static_cast<uint8_t*>(heap_caps_malloc(output, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!decoder->bytes) return false;
  decoder->reservation.allocated(0, output);
  decoder->stream.zalloc = allocateInflate;
  decoder->stream.zfree = freeInflate;
  decoder->stream.opaque = decoder;
  if (inflateInit(&decoder->stream) != Z_OK) return false;
  decoder->initialized = true;
  decoder->stream.next_in = const_cast<Bytef*>(entry.data);
  decoder->stream.avail_in = entry.packedSize;
  return true;
}
void closeFile() { file.close(); }
void release() { closeFile(); releaseDecoder(); delete lease; lease = nullptr; }
void failure(const char* reason) {
  closeFile();
  if (temporary.length()) tab5FS.remove(temporary);
  temporary = "";
  release(); phase = Phase::Idle; message = reason;
}
bool parents(const char* path) {
  String directory(path);
  for (unsigned p = 1; p < directory.length(); ++p) {
    if (directory[p] != '/') continue;
    const String parent = directory.substring(0, p);
    if (tab5FS.exists(parent)) {
      File check = tab5FS.open(parent, FILE_READ);
      if (!check || !check.isDirectory()) return false;
    } else if (!tab5FS.mkdir(parent)) return false;
  }
  return true;
}
void next() { release(); temporary = ""; ++index; phase = Phase::Select; }
}
bool busy() { return phase != Phase::Idle; }
const char* status() { return message; }
size_t created() { return restored; }
size_t checked() { return index; }
size_t total() { return sizeof(TAB5_SYSTEM_RESOURCES) / sizeof(TAB5_SYSTEM_RESOURCES[0]); }
bool start(bool sdMounted) {
  if (busy()) return false;
  if (!sdMounted) { message = "Insérez une microSD lisible"; return false; }
  index = restored = position = 0; temporary = "";
  phase = Phase::Select; message = "Restauration des ressources système manquantes";
  return true;
}
void cancel() { if (busy()) failure("Réparation des ressources annulée"); }
void service() {
  if (!busy()) return;
  if (phase == Phase::Select) {
    if (index >= total()) { phase = Phase::Idle; message = "Ressources système disponibles - fichiers existants conservés"; return; }
    lease = new(std::nothrow) Tab5StorageUse(true);
    if (!lease || !*lease) { failure("Stockage occupé - réessayez la réparation"); return; }
    const auto& entry = TAB5_SYSTEM_RESOURCES[index];
    // Existing files, even empty/custom files, belong to the user. This is
    // missing-file recovery, not a forced update or an integrity repair.
    if (tab5FS.exists(entry.path)) { next(); return; }
    if (entry.size > MaxResource || entry.packedSize > MaxResource + 1024 || !entry.packedSize) {
      failure("Vérification de la ressource système échouée"); return;
    }
    if (!beginDecode(entry)) { failure("Mémoire insuffisante"); return; }
    if (!parents(entry.path)) { failure("Création du dossier système impossible"); return; }
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
      temporary = String(entry.path) + ".tab5-repair-" + String(millis()) + "-" + String(attempt) + ".tmp";
      if (!tab5FS.exists(temporary)) break;
      temporary = "";
    }
    if (!temporary.length()) { failure("Aucun nom temporaire disponible pour la réparation"); return; }
    file = tab5FS.open(temporary, FILE_WRITE);
    if (!file) { failure("Écriture des ressources impossible - vérifiez l’espace libre"); return; }
    position = 0; phase = Phase::Decode; return;
  }
  const auto& entry = TAB5_SYSTEM_RESOURCES[index];
  if (phase == Phase::Decode) {
    auto& stream = decoder->stream;
    const size_t before = stream.total_out;
    stream.next_out = decoder->bytes + before;
    stream.avail_out = std::min(size_t(4096), entry.size + 1 - before);
    const int result = inflate(&stream, Z_NO_FLUSH);
    if (stream.total_out > entry.size ||
        (result != Z_OK && result != Z_STREAM_END) ||
        (result == Z_OK && stream.total_out == before)) {
      failure("Vérification de la ressource système échouée"); return;
    }
    if (result == Z_STREAM_END) {
      // zlib validates the checksum. Require exact lengths as well: neither
      // a truncated stream nor extra compressed/output bytes are accepted.
      if (stream.total_out != entry.size || stream.total_in != entry.packedSize) {
        failure("Vérification de la ressource système échouée"); return;
      }
      inflateEnd(&stream); decoder->initialized = false;
      decoder->reservation.release();
      phase = Phase::Write;
    }
    return;
  }
  const size_t count = std::min(size_t(4096), entry.size - position);
  if (phase == Phase::Write) {
    if (count && file.write(decoder->bytes + position, count) != count) {
      failure("Échec d’écriture des ressources - vérifiez l’espace libre"); return;
    }
    position += count;
    if (position == entry.size) {
      // Arduino VFS flush is void and does not propagate fflush/fsync into
      // Print::getWriteError(). Capture errno before size/close can change it.
      errno = 0; file.flush();
      bool ok = errno == 0 && !file.getWriteError() && file.size() == entry.size;
      errno = 0; closeFile(); ok = ok && errno == 0;
      if (!ok) { failure("Écriture incomplète de la ressource système"); return; }
      file = tab5FS.open(temporary, FILE_READ);
      if (!file || file.size() != entry.size) { failure("Vérification de la ressource système échouée"); return; }
      position = 0; phase = Phase::Verify;
    }
    return;
  }
  if (phase == Phase::Verify) {
    // Bounded stack use, with a readback against the firmware bytes.
    uint8_t buffer[512];
    const size_t bytes = std::min(sizeof(buffer), entry.size - position);
    if (bytes && (file.read(buffer, bytes) != bytes || memcmp(buffer, decoder->bytes + position, bytes))) {
      failure("Vérification de la ressource système échouée"); return;
    }
    position += bytes;
    if (position == entry.size) { closeFile(); phase = Phase::Commit; }
    return;
  }
  if (phase == Phase::Commit) {
    // Another client may have created the destination since selection. FAT
    // rename also refuses an existing destination, so it cannot replace it.
    if (tab5FS.exists(entry.path)) { tab5FS.remove(temporary); next(); return; }
    if (!tab5FS.rename(temporary, entry.path)) { failure("Installation de la ressource système impossible"); return; }
    ++restored; next();
  }
}
}
