#include "Tab5VolumeFS.h"
#include "Tab5FileShare.h"
#include "Tab5Task.h"
#include "Tab5FileSharePage.h"
#include "Tab5WebLocale.h"
#include "Tab5Localization.h"
#include "Tab5Storage.h"
#include "Tab5NetworkGate.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Private OS driver control. This class is not part of the application ABI.
class Tab5FileShareNetworkTransition {
 public:
  static const char* start(const char* ssid, const char* password, bool& attempted) {
    Tab5NetworkTransition transition("partage Wi-Fi", 2000);
    if (!transition || tab5NetworkEnabled() || WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF)
      return "Désactivez le Wi-Fi avant de créer le réseau direct";
    attempted = true;
    return WiFi.softAP(ssid, password, 1, 0, 1) ? nullptr : "Création du réseau direct impossible";
  }
  static bool stop(bool attempted) {
    if (attempted && WiFi.getMode() == WIFI_AP) {
      WiFi.softAPdisconnect(true);
      return WiFi.getMode() == WIFI_OFF;
    }
    return true;
  }
};

namespace {
constexpr uint32_t SessionMs = 30U * 60U * 1000U;
constexpr size_t MaxPath = 384, Block = 16384;
constexpr uint16_t Port = 8080;
std::atomic<bool> busy{false}, enabled{false}, ready{false}, direct{false};
std::atomic<uint32_t> code{0}, startedAt{0};
std::atomic<const char*> lastError{""};
char shareUrl[48]{}, shareSsid[24]{}, sharePassword[17]{};
enum class Action { Mkdir, Rename, Delete, Upload };
struct Context {
  char head[4096], path[MaxPath + 5], other[MaxPath + 5], temp[MaxPath + 5], backup[MaxPath + 5];
  char output[32768], data[Block], token[33], host[32];
  uint32_t address, mask;
  unsigned denied;
  bool ap;
  char* page;size_t pageLength;
  int socket;
  size_t bodyAt, buffered, contentLength;
  char* method;
  char* target;
  char* cookie;
  char* hostHeader;
  bool mutationHeader;
  Action action;
  uint32_t expected;
  unsigned result;
  const char* resultKey;
  struct stat original;
  bool existed, permanent;
};
std::atomic<Context*> pending{nullptr};
// 1 waiting, 2 owned by UI, 3 completed. Only the worker frees Context.
std::atomic<unsigned> commitState{0};

bool live() { return enabled.load() && uint32_t(millis() - startedAt.load()) < SessionMs; }
bool networkLive(Context& c) {
  return live() && (c.ap ? (WiFi.getMode() & WIFI_AP) != 0 :
      tab5NetworkEnabled() && WiFi.status() == WL_CONNECTED && uint32_t(WiFi.localIP()) == c.address);
}
bool waitSocket(int fd, bool writing, uint32_t deadline) {
  while (live() && int32_t(deadline - millis()) > 0) {
    fd_set set; FD_ZERO(&set); FD_SET(fd, &set);
    timeval t{0, 100000};
    int n = select(fd + 1, writing ? nullptr : &set, writing ? &set : nullptr, nullptr, &t);
    if (n > 0) return true;
    if (n < 0 && errno != EINTR) return false;
  }
  return false;
}
bool sendAll(Context& c, const void* data, size_t size) {
  const char* bytes = static_cast<const char*>(data);
  uint32_t deadline = millis() + 5000;
  while (size && live()) {
    if (!waitSocket(c.socket, true, deadline)) return false;
    int n = send(c.socket, bytes, std::min<size_t>(size, Block), MSG_DONTWAIT);
    if (n <= 0) { if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue; return false; }
    bytes += n; size -= n; deadline = millis() + 5000;
  }
  return !size;
}
bool headers(Context& c, unsigned status, const char* type, size_t size, const char* extra = "") {
  char text[1280];
  int n = snprintf(text, sizeof text,
    "HTTP/1.1 %u Result\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: close\r\n"
    "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n"
    "X-Frame-Options: DENY\r\n%s\r\n", status, type, static_cast<unsigned long>(size), extra);
  return n > 0 && size_t(n) < sizeof text && sendAll(c, text, n);
}
void reply(Context& c, unsigned status, const char* body, const char* extra = "") {
  if (headers(c, status, "application/json; charset=utf-8", strlen(body), extra)) sendAll(c, body, strlen(body));
}
void error(Context& c, unsigned status, const char* key) {
  char json[96]; snprintf(json, sizeof json, "{\"error\":\"%s\"}", key); reply(c, status, json);
}
void ioError(Context& c, const char* path) {
  int e = errno;
  if (e == ENOENT || e == ENOTDIR) error(c, 404, "missing");
  else if (e == EEXIST || e == ENOTEMPTY) error(c, 409, "exists");
  else if (e == ENOSPC) error(c, 507, "full");
  else { tab5StorageFault("Wi-Fi files", path, e); error(c, 503, "storage"); }
}
int digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool decimal(const char* s, uint32_t& result) {
  if (!s || !*s) return false;
  uint32_t n = 0;
  for (; *s; ++s) {
    if (*s < '0' || *s > '9' || n > (UINT32_MAX - unsigned(*s - '0')) / 10U) return false;
    n = n * 10U + (*s - '0');
  }
  result = n; return true;
}
bool parameter(Context& c, const char* name, char* out, size_t capacity) {
  const char* q = strchr(c.target, '?');
  if (!q) return false;
  size_t n = strlen(name);
  for (++q; *q;) {
    const char* end = strchr(q, '&'); if (!end) end = q + strlen(q);
    if (size_t(end - q) > n && !strncmp(q, name, n) && q[n] == '=') {
      size_t used = 0;
      for (q += n + 1; q < end; ++q) {
        unsigned char ch = *q;
        if (ch == '%') {
          if (end - q < 3 || digit(q[1]) < 0 || digit(q[2]) < 0) return false;
          ch = (digit(q[1]) << 4) | digit(q[2]); q += 2;
        } else if (ch == '+') ch = ' ';
        if (!ch || ch < 32 || ch == 127 || used + 1 >= capacity) return false;
        out[used++] = ch;
      }
      out[used] = 0; return true;
    }
    q = *end ? end + 1 : end;
  }
  return false;
}
bool pathParameter(Context& c, const char* name, char* out) {
  memcpy(out, "/sd", 3);
  if (!parameter(c, name, out + 3, MaxPath + 1) || out[3] != '/') return false;
  const char* part = out + 4;
  for (const char* p = part;; ++p) {
    if (*p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') return false;
    if (*p && *p != '/') continue;
    const size_t n = p - part;
    if ((!n && *p) || (n == 1 && *part == '.') || (n == 2 && part[0] == '.' && part[1] == '.') ||
        (n && (part[n-1] == '.' || part[n-1] == ' '))) return false;
    if (!*p) return true;
    part = p + 1;
  }
}
bool endpoint(Context& c, const char* path) {
  size_t n = strlen(path);
  return !strncmp(c.target, path, n) && (!c.target[n] || c.target[n] == '?');
}
bool rootPath(const char* p) { return !strcmp(p, "/sd/"); }
bool safeChain(const char* path) {
  // FAT has no links; reject them as well when another mounted FS supplies them.
  char p[MaxPath + 5]; snprintf(p, sizeof p, "%s", path);
  for (char* end = p + 4;; ++end) {
    if (*end && *end != '/') continue;
    char saved = *end; *end = 0; struct stat st{};
    #if defined(ARDUINO_ARCH_ESP32)
    int result = stat(p, &st);
#else
    int result = lstat(p, &st);
#endif
    *end = saved;
    if (!result && S_ISLNK(st.st_mode)) return false;
    if (result && errno != ENOENT) return false;
    if (!saved) break;
  }
  return true;
}
uint32_t identity(const struct stat& st) {
  uint64_t fields[] = {uint64_t(st.st_size), uint64_t(st.st_mtime), uint64_t(st.st_mode), uint64_t(st.st_ino)};
  uint32_t hash = 2166136261U;
  for (uint64_t value : fields) for (unsigned i = 0; i < 8; ++i) {
    hash = (hash ^ uint8_t(value)) * 16777619U; value >>= 8;
  }
  return hash ? hash : 1U;
}
bool readHeader(Context& c) {
  size_t used = 0; uint32_t deadline = millis() + 5000;
  c.cookie = c.hostHeader = nullptr; c.mutationHeader = false; c.contentLength = 0;
  while (used < sizeof(c.head) - 1) {
    if (!waitSocket(c.socket, false, deadline)) return false;
    int n = recv(c.socket, c.head + used, sizeof(c.head) - 1 - used, MSG_DONTWAIT);
    if (n <= 0) return false;
    used += n; c.head[used] = 0;
    char* end = strstr(c.head, "\r\n\r\n");
    if (!end) continue;
    c.bodyAt = end + 4 - c.head; c.buffered = used - c.bodyAt;
    *end = 0;
    char* lineEnd = strstr(c.head, "\r\n");
    if (!lineEnd) return false;
    *lineEnd = 0; c.method = c.head;
    char* space = strchr(c.head, ' '); if (!space) return false;
    *space = 0; c.target = space + 1; space = strchr(c.target, ' '); if (!space) return false;
    *space = 0;
    if (strcmp(space + 1, "HTTP/1.1") || strlen(c.target) > 1400) return false;
    bool haveLength = false;
    for (char* p = lineEnd + 2; p < end;) {
      char* next = strstr(p, "\r\n"); if (!next) next = end;
      *next = 0; char* colon = strchr(p, ':'); if (!colon) return false;
      *colon++ = 0; while (*colon == ' ' || *colon == '\t') ++colon;
      if (!strcasecmp(p, "Content-Length")) {
        uint32_t length = 0;
        if (haveLength || !decimal(colon, length)) return false;
        haveLength = true; c.contentLength = length;
      } else if (!strcasecmp(p, "Transfer-Encoding")) return false;
      else if (!strcasecmp(p, "Cookie")) { if (c.cookie) return false; c.cookie = colon; }
      else if (!strcasecmp(p, "Host")) { if (c.hostHeader) return false; c.hostHeader = colon; }
      else if (!strcasecmp(p, "X-Tab5-Request")) c.mutationHeader = !strcmp(colon, "1");
      p = next + 2;
    }
    if (!c.hostHeader || strcmp(c.hostHeader, c.host) || c.buffered > c.contentLength) return false;
    return true;
  }
  return false;
}
bool readBody(Context& c, char* out, size_t size) {
  size_t done = 0;
  if (c.buffered) {
    size_t take = std::min(c.buffered, size); memcpy(out, c.head + c.bodyAt, take);
    c.bodyAt += take; c.buffered -= take; done += take;
  }
  uint32_t deadline = millis() + 8000;
  while (done < size && live()) {
    if (!waitSocket(c.socket, false, deadline)) return false;
    int n = recv(c.socket, out + done, size - done, MSG_DONTWAIT);
    if (n <= 0) return false;
    done += n; deadline = millis() + 8000;
  }
  return done == size;
}
bool authenticated(Context& c) {
  if (!c.cookie) return false;
  const char* p = c.cookie;
  while (*p) {
    while (*p == ' ' || *p == ';') ++p;
    const char* end = strchr(p, ';'); if (!end) end = p + strlen(p);
    if (end - p == 36 && !strncmp(p, "t5s=", 4)) {
      unsigned difference = 0; for (unsigned i = 0; i < 32; ++i) difference |= unsigned(p[i+4] ^ c.token[i]);
      return !difference;
    }
    p = end;
  }
  return false;
}
size_t jsonString(char* out, size_t capacity, const char* s) {
  size_t n = 0; if (capacity < 3) return 0; out[n++] = '"';
  for (; *s; ++s) {
    unsigned char ch = *s;
    if (n + 7 >= capacity) return 0;
    if (ch == '"' || ch == '\\') { out[n++] = '\\'; out[n++] = ch; }
    else if (ch < 32) n += snprintf(out + n, capacity - n, "\\u%04x", ch);
    else out[n++] = ch;
  }
  out[n++] = '"'; out[n] = 0; return n;
}
void listing(Context& c) {
  char offsetText[16]; uint32_t offset = 0;
  if (parameter(c, "offset", offsetText, sizeof offsetText) && !decimal(offsetText, offset)) { error(c, 400, "request"); return; }
  if (offset > 100000) { error(c, 400, "request"); return; }
  Tab5StorageUse use; if (!use) { error(c, 503, "busy"); return; }
  if (!safeChain(c.path)) { error(c, 400, "path"); return; }
  DIR* dir = opendir(c.path); if (!dir) { ioError(c, c.path); return; }
  size_t used = snprintf(c.output, sizeof c.output, "{\"entries\":[");
  unsigned index = 0, count = 0, cursor = offset; bool more = false, fail = false; uint32_t began = millis();
  size_t parentSize = strlen(c.path);
  while (live()) {
    errno = 0; dirent* entry = readdir(dir);
    if (!entry) { if (errno) fail = true; break; }
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
    if ((index & 15) == 0) { vTaskDelay(1); if (millis() - began > 3000) { fail = true; break; } }
    if (index++ < offset) continue;
    if (count == 32) { more = true; break; }
    cursor = index;
    if (parentSize + strlen(entry->d_name) + 2 >= sizeof c.other) continue;
    snprintf(c.other, sizeof c.other, "%s%s%s", c.path, c.path[parentSize-1] == '/' ? "" : "/", entry->d_name);
    struct stat st{}; if (stat(c.other, &st)) { fail = true; break; }
    if (count) c.output[used++] = ',';
    used += snprintf(c.output + used, sizeof c.output - used, "{\"name\":");
    size_t escaped = jsonString(c.output + used, sizeof c.output - used, entry->d_name);
    if (!escaped) { fail = true; break; } used += escaped;
    used += snprintf(c.output + used, sizeof c.output - used, ",\"dir\":%s,\"size\":%llu,\"id\":%lu}", S_ISDIR(st.st_mode) ? "true" : "false", static_cast<unsigned long long>(st.st_size), static_cast<unsigned long>(identity(st)));
    ++count;
  }
  closedir(dir);
  if (fail) { error(c, 503, "storage"); return; }
  snprintf(c.output + used, sizeof c.output - used, "],\"next\":%lu,\"more\":%s}", static_cast<unsigned long>(cursor), more ? "true" : "false");
  reply(c, 200, c.output);
}
void metadata(Context& c) {
  Tab5StorageUse use; if (!use) { error(c, 503, "busy"); return; }
  if (!safeChain(c.path)) { error(c, 400, "path"); return; }
  struct stat st{}; if (stat(c.path, &st)) { ioError(c, c.path); return; }
  char out[128]; snprintf(out, sizeof out, "{\"dir\":%s,\"size\":%llu,\"id\":%lu}", S_ISDIR(st.st_mode) ? "true" : "false", static_cast<unsigned long long>(st.st_size), static_cast<unsigned long>(identity(st)));
  reply(c, 200, out);
}
void download(Context& c) {
  Tab5StorageUse use; if (!use) { error(c, 503, "busy"); return; }
  if (!safeChain(c.path)) { error(c, 400, "path"); return; }
  int fd = open(c.path, O_RDONLY); if (fd < 0) { ioError(c, c.path); return; }
  struct stat st{};
  if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 || uint64_t(st.st_size) > INT32_MAX) {
    close(fd); error(c, 413, "large"); return;
  }
  char extra[896]; size_t used = snprintf(extra, sizeof extra, "Content-Disposition: attachment; filename=\"download\"; filename*=UTF-8''");
  const char* name = strrchr(c.path, '/'); name = name ? name + 1 : c.path;
  for (; *name && used + 5 < sizeof extra; ++name) used += snprintf(extra + used, sizeof extra - used, "%%%02X", unsigned(static_cast<unsigned char>(*name)));
  snprintf(extra + used, sizeof extra - used, "\r\n");
  bool ok = headers(c, 200, "application/octet-stream", st.st_size, extra);
  size_t left = st.st_size;
  while (ok && left && live()) {
    size_t want = std::min(left, Block); ssize_t n = read(fd, c.data, want);
    if (n <= 0) { ok = false; break; }
    ok = sendAll(c, c.data, n); left -= n; vTaskDelay(1);
  }
  close(fd);
}
bool existingIdentity(Context& c, const struct stat& st) {
  char value[16]; uint32_t expected = 0;
  return parameter(c, "id", value, sizeof value) && decimal(value, expected) && expected == identity(st);
}
bool queueCommit(Context& c) {
  c.result = 503; c.resultKey = "busy"; c.backup[0] = 0;
  pending.store(&c, std::memory_order_release);
  commitState.store(1, std::memory_order_release);
  const uint32_t began = millis();
  while (true) {
    unsigned state = commitState.load(std::memory_order_acquire);
    if (state == 3) { commitState = 0; pending = nullptr; return c.result == 200; }
    if ((!live() || millis() - began > 10000U) && state == 1) {
      unsigned expected = 1;
      if (commitState.compare_exchange_strong(expected, 0)) { pending = nullptr; return false; }
    }
    // Once the UI claims it, finish the short SD commit before releasing Context.
    vTaskDelay(1);
  }
}
void commitReply(Context& c) {
  if (c.result != 200) {
    if (!c.backup[0]) { error(c, c.result, c.resultKey); return; }
    size_t n = snprintf(c.output, sizeof c.output, "{\"error\":\"%s\",\"backup\":", c.resultKey);
    n += jsonString(c.output + n, sizeof c.output - n, c.backup + 3);
    snprintf(c.output + n, sizeof c.output - n, "}"); reply(c, c.result, c.output); return;
  }
  if (!c.backup[0]) { reply(c, 200, "{}"); return; }
  size_t n = snprintf(c.output, sizeof c.output, "{\"backup\":");
  n += jsonString(c.output + n, sizeof c.output - n, c.backup + 3);
  snprintf(c.output + n, sizeof c.output - n, "}"); reply(c, 200, c.output);
}
void mutation(Context& c) {
  if (rootPath(c.path)) { error(c, 400, "path"); return; }
  c.action = endpoint(c, "/api/mkdir") ? Action::Mkdir : endpoint(c, "/api/delete") ? Action::Delete : Action::Rename;
  if (c.action != Action::Mkdir) {
    char id[16];
    if (!parameter(c, "id", id, sizeof id) || !decimal(id, c.expected)) { error(c, 400, "request"); return; }
  }
  if (c.action == Action::Rename && (!pathParameter(c, "to", c.other) || rootPath(c.other))) { error(c, 400, "path"); return; }
  char permanent[4];
  c.permanent = parameter(c, "permanent", permanent, sizeof permanent) && !strcmp(permanent, "1") && !strncmp(c.path, "/sd/.Tab5Trash/", sizeof("/sd/.Tab5Trash/") - 1);
  queueCommit(c); commitReply(c);
}
void upload(Context& c) {
  if (rootPath(c.path) || c.contentLength > INT32_MAX) { error(c, 413, "large"); return; }
  {
  Tab5StorageUse use; if (!use) { error(c, 503, "busy"); return; }
  if (!safeChain(c.path)) { error(c, 400, "path"); return; }
  struct stat& original = c.original; bool exists = !stat(c.path, &original);
  c.existed = exists;
  if ((!exists && errno != ENOENT) || (exists && !S_ISREG(original.st_mode))) { error(c, 409, "exists"); return; }
  if (exists && !existingIdentity(c, original)) { error(c, 409, "changed"); return; }
  size_t parent = strrchr(c.path, '/') - c.path + 1;
  if (parent + 24 >= sizeof c.temp) { error(c, 400, "path"); return; }
  memcpy(c.temp, c.path, parent); snprintf(c.temp + parent, sizeof c.temp - parent, ".t5upload-%08lx", static_cast<unsigned long>(esp_random()));
  int fd = open(c.temp, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (fd < 0) { ioError(c, c.temp); return; }
  size_t left = c.contentLength; bool ok = true;
  while (left && live()) {
    size_t want = std::min(left, Block);
    if (!readBody(c, c.data, want)) { ok = false; break; }
    size_t written = 0;
    while (written < want) {
      ssize_t n = write(fd, c.data + written, want - written);
      if (n <= 0) { ok = false; break; } written += n;
    }
    if (!ok) break; left -= want; vTaskDelay(1);
  }
  if (left || !live()) ok = false;
  if (ok && fsync(fd)) ok = false;
  if (close(fd)) ok = false;
  if (!ok) { unlink(c.temp); error(c, 503, "transfer"); return; }
  }
  c.action = Action::Upload;
  if (!queueCommit(c)) {
    Tab5StorageUse use;
    if (use) unlink(c.temp);
  }
  commitReply(c);
}
bool trashDestination(Context& c) {
  constexpr char folder[] = "/sd/.Tab5Trash";
  struct stat st{};
  if (stat(folder, &st)) {
    if (errno != ENOENT || mkdir(folder, 0777)) return false;
  } else if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return false; }
  if (!safeChain(folder)) { errno = EINVAL; return false; }
  const char* name = strrchr(c.path, '/') + 1;
  // A unique directory keeps even a 255-byte original basename intact.
  for (unsigned attempt = 0; attempt < 4; ++attempt) {
    snprintf(c.backup, sizeof c.backup, "%s/%08lx", folder, static_cast<unsigned long>(esp_random()));
    if (!mkdir(c.backup, 0777)) {
      size_t n = strlen(c.backup);
      snprintf(c.backup + n, sizeof c.backup - n, "/%s", name);
      return true;
    }
    if (errno != EEXIST) return false;
  }
  c.backup[0] = 0; errno = EEXIST; return false;
}
void executeCommit(Context& c) {
  struct NotifyMutation {Context& context;~NotifyMutation(){if(context.result==200)tab5FsChanged();}} notify{c};
  c.result = 503; c.resultKey = "storage";
  if (!live()) { c.resultKey = "transfer"; return; }
  if (!safeChain(c.path)) { c.result = 400; c.resultKey = "path"; return; }
  if (c.action == Action::Mkdir) {
    if (!mkdir(c.path, 0777)) c.result = 200;
    else if (errno == EEXIST) { c.result = 409; c.resultKey = "exists"; }
    return;
  }
  struct stat st{}; bool exists = !stat(c.path, &st);
  if (!exists && errno != ENOENT) return;
  if (c.action == Action::Upload) {
    if (exists != c.existed || (exists && (st.st_size != c.original.st_size || st.st_mtime != c.original.st_mtime || st.st_mode != c.original.st_mode))) {
      c.result = 409; c.resultKey = "changed"; return;
    }
    if (exists && (!trashDestination(c) || rename(c.path, c.backup))) { c.backup[0] = 0; return; }
    if (rename(c.temp, c.path)) {
      // Never remove the old file if rollback is impossible. It remains in Trash.
      if (exists && !rename(c.backup, c.path)) c.backup[0] = 0;
      return;
    }
    c.result = 200; return;
  }
  if (!exists) { c.result = 404; c.resultKey = "missing"; return; }
  if (c.expected != identity(st)) { c.result = 409; c.resultKey = "changed"; return; }
  if (c.action == Action::Delete) {
    if (S_ISDIR(st.st_mode)) {
      if (!rmdir(c.path)) c.result = 200;
      else if (errno == ENOTEMPTY || errno == EEXIST) { c.result = 409; c.resultKey = "exists"; }
    } else if (c.permanent) { if (!unlink(c.path)) c.result = 200; }
    else if (trashDestination(c) && !rename(c.path, c.backup)) c.result = 200;
    else c.backup[0] = 0;
    return;
  }
  if (!safeChain(c.other)) { c.result = 400; c.resultKey = "path"; return; }
  struct stat target{};
  if (!stat(c.other, &target)) { c.result = 409; c.resultKey = "exists"; return; }
  if (errno != ENOENT) return;
  if (!rename(c.path, c.other)) c.result = 200;
}

void request(Context& c) {
  if (!readHeader(c)) { error(c, 400, "request"); return; }
  if (!strcmp(c.method, "GET") && endpoint(c, "/")) {
    if (headers(c, 200, "text/html; charset=utf-8", c.pageLength, "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; form-action 'none'; base-uri 'none'; frame-ancestors 'none'\r\n")) sendAll(c, c.page, c.pageLength);
    return;
  }
  if (!strcmp(c.method, "POST") && endpoint(c, "/login") && c.mutationHeader) {
    char entered[7]{}; uint32_t supplied = 0;
    bool valid = c.contentLength == 6 && readBody(c, entered, 6) && decimal(entered, supplied) && supplied == code.load();
    if (!valid) { error(c, 403, "auth"); if (++c.denied >= 8) enabled = false; return; }
    char cookie[128]; snprintf(cookie, sizeof cookie, "Set-Cookie: t5s=%s; HttpOnly; SameSite=Strict; Path=/; Max-Age=1800\r\n", c.token);
    reply(c, 200, "{}", cookie); return;
  }
  if (!authenticated(c)) { error(c, 403, "auth"); return; }
  if (!pathParameter(c, "path", c.path)) { error(c, 400, "path"); return; }
  if (!strcmp(c.method, "GET") && endpoint(c, "/api/list")) listing(c);
  else if (!strcmp(c.method, "GET") && endpoint(c, "/api/download")) download(c);
  else if (!strcmp(c.method, "GET") && endpoint(c, "/api/stat")) metadata(c);
  else if (!c.mutationHeader) error(c, 403, "request");
  else if (!strcmp(c.method, "PUT") && endpoint(c, "/api/upload")) upload(c);
  else if (!strcmp(c.method, "POST") && !c.contentLength &&
           (endpoint(c, "/api/mkdir") || endpoint(c, "/api/delete") || endpoint(c, "/api/rename"))) mutation(c);
  else error(c, 404, "request");
}
void finishSession(Context& c, int server, bool apAttempted) {
  ready = false; enabled = false;
  if (server >= 0) close(server);
  heap_caps_free(c.page); heap_caps_free(&c);
  code = 0;
  const char* originalError = lastError.load();
  // Keep driver ownership until it really stops. A transient SDK failure
  // must not orphan an AP after the worker and its Stop button disappear.
  while (!Tab5FileShareNetworkTransition::stop(apAttempted)) {
    lastError = "Arrêt réseau en attente — réessayez";
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  lastError = originalError;
  busy.store(false, std::memory_order_release);
}
void worker(void* arg) {
  Context& c = *static_cast<Context*>(arg);
  int server = -1; bool apAttempted = false;
  if (c.ap) {
    const char* error = Tab5FileShareNetworkTransition::start(shareSsid, sharePassword, apAttempted);
    if (error) { lastError = error; goto cleanup; }
    c.address = uint32_t(WiFi.softAPIP()); c.mask = uint32_t(IPAddress(255,255,255,0));
  } else {
    if (!tab5NetworkEnabled() || WiFi.status() != WL_CONNECTED) { lastError = "Connectez la tablette au Wi-Fi"; goto cleanup; }
    c.address = uint32_t(WiFi.localIP()); c.mask = uint32_t(WiFi.subnetMask());
  }
  {
    IPAddress address(c.address);
    snprintf(c.host, sizeof c.host, "%u.%u.%u.%u:%u", address[0], address[1], address[2], address[3], Port);
    snprintf(shareUrl, sizeof shareUrl, "http://%s", c.host);
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { lastError = "Ouverture du partage impossible"; goto cleanup; }
    int reuse = 1; setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_port = htons(Port); local.sin_addr.s_addr = c.address;
    if (bind(server, reinterpret_cast<sockaddr*>(&local), sizeof local) || listen(server, 2)) { lastError = "Ouverture du partage impossible"; goto cleanup; }
    ready.store(true, std::memory_order_release);
    while (networkLive(c)) {
      if (!waitSocket(server, false, millis() + 300)) continue;
      sockaddr_in peer{}; socklen_t length = sizeof peer;
      c.socket = accept(server, reinterpret_cast<sockaddr*>(&peer), &length);
      if (c.socket < 0) continue;
      if ((peer.sin_addr.s_addr & c.mask) == (c.address & c.mask)) request(c);
      shutdown(c.socket, SHUT_RDWR); close(c.socket); c.socket = -1;
      vTaskDelay(1);
    }
  }
cleanup:
  finishSession(c, server, apAttempted);
  vTaskDelete(nullptr);
}
}
bool tab5FileShareStart(bool accessPoint, bool french) {
  (void)french; // Legacy caller compatibility; use the effective OS locale below.
  bool expected = false;
  if (!busy.compare_exchange_strong(expected, true)) return false;
  ready = false; lastError = ""; shareUrl[0] = shareSsid[0] = sharePassword[0] = 0;
  if ((!accessPoint && (!tab5NetworkEnabled() || WiFi.status() != WL_CONNECTED)) ||
      (accessPoint && (tab5NetworkEnabled() || WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF))) {
    lastError = accessPoint ? "Désactivez le Wi-Fi avant de créer le réseau direct" : "Connectez la tablette au Wi-Fi";
    busy = false; return false;
  }
  Context* c = static_cast<Context*>(heap_caps_calloc(1, sizeof(Context), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!c) { lastError = "Mémoire indisponible"; busy = false; return false; }
  // This UI-owned snapshot is immutable for the worker/session. No dictionary
  // pointer escapes the locale mutex and no locale SD read occurs in the worker.
  constexpr size_t PageCapacity=32768;
  c->page=static_cast<char*>(heap_caps_malloc(PageCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  const String language=Tab5Locale.effectiveId();
  if(c->page)c->pageLength=Tab5WebLocale::page(c->page,PageCapacity,language.c_str(),c->data,sizeof(c->data),[](const char* key,char* out,size_t cap){return Tab5Locale.copyText(key,out,cap);});
  if(!c->pageLength){heap_caps_free(c->page);heap_caps_free(c);lastError="Mémoire indisponible";busy=false;return false;}
  constexpr char hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < 32; ++i) c->token[i] = hex[esp_random() & 15];
  c->ap = accessPoint; c->socket = -1;
  code = 100000U + esp_random() % 900000U;
  if (accessPoint) {
    snprintf(shareSsid, sizeof shareSsid, "Tab5-%06lu", static_cast<unsigned long>(esp_random() % 1000000U));
    for (unsigned i = 0; i < 16; ++i) sharePassword[i] = hex[esp_random() & 15];
  }
  startedAt = millis(); enabled = true; direct = accessPoint;
  if (tab5CreateTask(worker, "sd-share", 8192, c, 1, nullptr, 0) != pdPASS) {
    heap_caps_free(c->page);heap_caps_free(c); enabled = false; code = 0; busy = false; lastError = "Mémoire indisponible"; return false;
  }
  return true;
}
void tab5FileShareService() {
  if (commitState.load(std::memory_order_acquire) != 1) return;
  Tab5StorageRecovery exclusive;
  if (!exclusive) return;
  unsigned expected = 1;
  if (!commitState.compare_exchange_strong(expected, 2)) return;
  Context* c = pending.load(std::memory_order_acquire);
  executeCommit(*c);
  commitState.store(3, std::memory_order_release);
}
void tab5FileShareStop() { enabled = false; }
bool tab5FileShareBusy() { return busy.load(std::memory_order_acquire); }
bool tab5FileShareActive() { return ready.load(std::memory_order_acquire) && live(); }
bool tab5FileShareAccessPoint() { return busy.load() && direct.load(); }
uint32_t tab5FileShareCode() { return busy.load() ? code.load() : 0; }
uint32_t tab5FileShareSecondsRemaining() { return live() ? (SessionMs - uint32_t(millis() - startedAt.load()) + 999) / 1000 : 0; }
const char* tab5FileShareUrl() { return ready.load(std::memory_order_acquire) ? shareUrl : ""; }
const char* tab5FileShareSsid() { return busy.load(std::memory_order_acquire) ? shareSsid : ""; }
const char* tab5FileSharePassword() { return busy.load(std::memory_order_acquire) ? sharePassword : ""; }
const char* tab5FileShareError() { return lastError.load(); }
