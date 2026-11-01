#include "Tab5Serial.h"
#include "Tab5SecureClient.h"
#include "Tab5CooperativeClient.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5AtomicFile.h"
#include "Tab5Map.h"
#include "Tab5MapImportPlan.h"
#include "Tab5Audio.h"
#include "Tab5ResourceManager.h"
#include "Tab5Task.h"
#include "Tab5TilePolicy.h"
#include "Tab5TileValidation.h"
#include "Tab5Localization.h"
#include "Tab5NetDiagnostics.h"
#include "Tab5Memory.h"
#include "Tab5NetworkGate.h"

#include <FS.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#ifndef ARDUINOJSON_ENABLE_STD_STREAM
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#endif
#ifndef ARDUINOJSON_ENABLE_STD_STRING
#define ARDUINOJSON_ENABLE_STD_STRING 0
#endif
#include "Tab5Json.h"
#include "Tab5MapSearchParser.h"
#include "Tab5GeoIndex.h"
#include "Tab5Meshtastic.h"
#include <Preferences.h>
#include <algorithm>
#include <esp_heap_caps.h>
#include <ctype.h>
#include <math.h>

struct Tab5MapConnection {
 Tab5CooperativeClient<NetworkClient> plain;Tab5CooperativeClient<Tab5SecureClient> secure;HTTPClient http;String origin;bool tls=false;uint32_t used=0;
 Tab5MapConnection(){secure.setHandshakeTimeout(8);}
 NetworkClient& client(){return tls?static_cast<NetworkClient&>(secure):plain;}
 bool retained() const { return origin.length() != 0; }
 void close(){http.setReuse(false);http.end();plain.stop();secure.stop();origin="";tab5NetworkSetBackgroundRetained(false);}
 bool reuse(const String& url){int start=url.indexOf("://");int end=url.indexOf('/',start+3);String next=end<0?url:url.substring(0,end);
  bool live=origin==next&&client().connected();if(!live){close();origin=next;tls=url.startsWith("https://");}used=millis();tab5NetworkSetBackgroundRetained(true);return live;}
 ~Tab5MapConnection(){close();}
};

struct Tab5MapRequestCancellation {
  Tab5MapConnection& connection;
  Tab5MapRequestCancellation(Tab5MapConnection& c, Tab5NetworkCancel cancel,
                            void* context) : connection(c) {
    connection.plain.setCancellation(cancel, context);
    connection.secure.setCancellation(cancel, context);
  }
  ~Tab5MapRequestCancellation() {
    connection.plain.clearCancellation();
    connection.secure.clearCancellation();
  }
};

namespace {

static constexpr char MAP_ROOT[] = "/System/maps";
static constexpr char MAP_CACHE_ROOT[] = "/System/maps/cache";
static constexpr char MAP_OFFLINE_ROOT[] = "/System/maps/offline";
static constexpr char MAP_REGION_REQUEST[] = "/System/maps/region_request.json";
static constexpr char MAP_STYLES_FILE[] = "/System/map_styles.json";
static constexpr char MAP_PLACES_FILE[] = "/System/map_places.json";
static constexpr char MAP_PREFS_NAMESPACE[] = "tab5os-map";
static constexpr size_t MAX_TILE_BYTES = tab5tile::MaxBytes;
static constexpr size_t MAX_SEARCH_BYTES = 128U * 1024U;
static constexpr size_t MAX_CONFIG_BYTES = 48U * 1024U;
static constexpr size_t MAX_PLACES_BYTES = 64U * 1024U;
static constexpr size_t MAP_SD_IO_CHUNK = 32U * 1024U;
static constexpr uint32_t MAP_HTTP_TIMEOUT_MS = 8000;
static constexpr uint32_t MAP_TASK_STACK = 16U * 1024U;
static constexpr UBaseType_t MAP_TASK_PRIORITY = 2;
static constexpr size_t MAP_MAX_OFFLINE_DIRECTORY = 2U * 1024U * 1024U;
static constexpr uint32_t T5MAP_VERSION = 1;
static constexpr uint32_t T5MAP_HEADER_SIZE = 128;
static constexpr char T5MAP_MAGIC[8] = {'T', '5', 'M', 'A', 'P', '0', '1', 0};

static bool mapConnectionFailureLogDue() {
  static uint32_t lastLogMs = 0;
  const uint32_t now = millis();
  if (lastLogMs && now - lastLogMs < 15000U) return false;
  lastLogMs = now ? now : 1U;
  return true;
}

class Tab5MapNetworkDiagnosticGuard {
 public:
  Tab5MapNetworkDiagnosticGuard() { tab5NetDiagnosticsMapBegin(); }
  ~Tab5MapNetworkDiagnosticGuard() { tab5NetDiagnosticsMapEnd(); }
};

struct __attribute__((packed)) T5MapFileHeader {
  char magic[8];
  uint32_t version;
  uint32_t headerSize;
  uint32_t entryCount;
  uint32_t directoryCount;
  uint32_t entriesPerBlock;
  uint32_t directoryOffset;
  uint32_t indexOffset;
  uint32_t dataOffset;
  uint64_t minKey;
  uint64_t maxKey;
  char cacheId[24];
  char name[40];
  uint8_t reserved[8];
};

static_assert(sizeof(T5MapFileHeader) == T5MAP_HEADER_SIZE,
              "En-tête T5MAP inattendu");

class PsramBufferStream : public Stream {
 public:
  explicit PsramBufferStream(size_t limit, size_t initial = 0,
                            Tab5NetworkCancel cancel = nullptr,
                            void* context = nullptr)
      : _buffer(limit, initial), _cancel(cancel), _context(context) {}

  bool valid() const { return !_buffer.exhausted(); }
  bool overflowed() const { return _buffer.overflowed() || _buffer.exhausted(); }
  const uint8_t* data() const { return _buffer.data(); }
  uint8_t* detach() { return _buffer.detach(); }
  size_t size() const { return _buffer.size(); }
  bool cancelled() const { return _cancelled; }

  size_t write(uint8_t value) override { return write(&value, 1); }

  size_t write(const uint8_t* data, size_t length) override {
    if (_cancel && _cancel(_context)) { _cancelled = true; return 0; }
    return _buffer.append(data, length) ? length : 0;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

 private:
  Tab5ByteBuffer _buffer;
  Tab5NetworkCancel _cancel;
  void* _context;
  bool _cancelled = false;
};

static bool readFileCooperatively(File& file, uint8_t* destination,
                                  size_t length) {
  if (!file || !destination || !length) return false;
  size_t total = 0;
  while (total < length) {
    const size_t remaining = length - total;
    const size_t chunk = remaining > MAP_SD_IO_CHUNK
        ? MAP_SD_IO_CHUNK : remaining;
    errno = 0;
    const size_t received = file.read(destination + total, chunk);
    if (!received || received > chunk) {
      if (!errno) errno = EIO;
      return false;
    }
    total += received;
    if (total < length) delay(1);
  }
  return true;
}

static bool writeFileCooperatively(File& file, const uint8_t* source,
                                   size_t length) {
  if (!file || !source || !length) return false;
  size_t total = 0;
  while (total < length) {
    const size_t remaining = length - total;
    const size_t chunk = remaining > MAP_SD_IO_CHUNK
        ? MAP_SD_IO_CHUNK : remaining;
    errno = 0;
    const size_t written = file.write(source + total, chunk);
    if (!written || written > chunk) {
      if (!errno) errno = EIO;
      return false;
    }
    total += written;
    if (total < length) delay(1);
  }
  return true;
}

static bool closeWrittenMapFile(File& file, bool written, size_t expected) {
  int error = written ? 0 : (errno ? errno : EIO);
  if (file.getWriteError() && !error) error = errno ? errno : EIO;
  errno = 0;
  file.flush();
  if (errno && !error) error = errno;
  errno = 0;
  const size_t actual = file.size();
  if (!error && (errno || actual != expected)) error = errno ? errno : EIO;
  errno = 0;
  file.close();
  if (errno && !error) error = errno;
  errno = error;
  return !error;
}

static bool prepareMapTemporary(const String& path) {
  bool present = false;
  if (tab5AtomicProbe(path.c_str(), present) &&
      (!present || tab5FS.remove(path.c_str()))) return true;
  tab5StorageFault("fichier temporaire", path.c_str(), errno ? errno : EIO);
  return false;
}

static bool publishMapFile(const String& temporary, const String& path) {
  const String backup = path + ".bak";
  if (!tab5AtomicReplace(temporary.c_str(), path.c_str(), backup.c_str())) {
    const int error = errno ? errno : EIO;
    tab5StorageFault("publication", path.c_str(), error);
    tab5FS.remove(temporary.c_str());
    errno = error;
    return false;
  }
  bool present = false;
  if (tab5AtomicProbe(backup.c_str(), present) && present)
    tab5FS.remove(backup.c_str());
  return true;
}

static bool ensureMapDirectory(const String& path) {
  errno = 0;
  if (tab5FS.mkdir(path.c_str())) return true;
  const int error = errno ? errno : EIO;
  if (error == EEXIST) {
    File existing = tab5FS.open(path.c_str(), FILE_READ);
    const bool directory = existing && existing.isDirectory();
    existing.close();
    if (directory) return true;
  }
  tab5StorageFault("dossier cache", path.c_str(), error);
  errno = error;
  return false;
}

static bool validTile(const uint8_t* data, size_t size) { return Tab5TileValidation::validate(data,size)==Tab5TileValidation::Result::Valid; }



static uint64_t readLe64(const uint8_t* data) {
  uint64_t value = 0;
  for (int index = 7; index >= 0; --index)
    value = (value << 8) | data[index];
  return value;
}

static bool readVarint(const uint8_t*& current, const uint8_t* end,
                       uint64_t& value) {
  value = 0;
  unsigned shift = 0;
  while (current < end && shift < 64) {
    const uint8_t byte = *current++;
    value |= static_cast<uint64_t>(byte & 0x7fU) << shift;
    if (!(byte & 0x80U)) return true;
    shift += 7;
  }
  return false;
}

static String trimmedConfigValue(String value) {
  value.trim();
  if (value.length() >= 2 &&
      ((value[0] == '"' && value[value.length() - 1] == '"') ||
       (value[0] == '\'' && value[value.length() - 1] == '\''))) {
    value = value.substring(1, value.length() - 1);
  }
  return value;
}

static bool safeIdentifier(const String& value) {
  if (!value.length() || value.length() > 23) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
      return false;
  }
  return true;
}

static void copyUtf8(char* destination, size_t capacity, String value) {
  if (!destination || !capacity) return;
  value.replace('\r', ' ');
  value.replace('\n', ' ');
  value.replace('\t', ' ');
  value.trim();
  size_t length = value.length();
  if (length >= capacity) length = capacity - 1;
  while (length > 0 && length < value.length() &&
         (static_cast<uint8_t>(value[length]) & 0xC0) == 0x80) {
    --length;
  }
  memcpy(destination, value.c_str(), length);
  destination[length] = 0;
}

static String urlEncoded(const char* text) {
  // Print.h réserve déjà ce nom générique pour l'affichage hexadécimal.
  static constexpr char URL_HEX_DIGITS[] = "0123456789ABCDEF";
  String encoded;
  if (!text) return encoded;
  encoded.reserve(strlen(text) * 3);
  for (const uint8_t* p = reinterpret_cast<const uint8_t*>(text); *p; ++p) {
    const uint8_t c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += URL_HEX_DIGITS[c >> 4];
      encoded += URL_HEX_DIGITS[c & 0x0F];
    }
  }
  return encoded;
}

}  // namespace

Tab5MapEngine Tab5Map;

bool Tab5MapEngine::allocateRuntimeStorage() {
  if (_styles && _places && _memoryTiles && _offlinePacks)
    return true;

  _styles = static_cast<MapStyle*>(heap_caps_calloc(
      TAB5_MAP_MAX_STYLES, sizeof(MapStyle),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  _places = static_cast<Tab5MapPlace*>(heap_caps_calloc(
      TAB5_MAP_MAX_PLACES, sizeof(Tab5MapPlace),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  _memoryTiles = static_cast<MemoryTile*>(heap_caps_calloc(
      MEMORY_TILE_COUNT, sizeof(MemoryTile),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  _offlinePacks = static_cast<OfflinePack*>(heap_caps_calloc(
      TAB5_MAP_MAX_OFFLINE_PACKS, sizeof(OfflinePack),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  if (_styles && _places && _memoryTiles && _offlinePacks)
    return true;

  if (_styles) heap_caps_free(_styles);
  if (_places) heap_caps_free(_places);
  if (_memoryTiles) heap_caps_free(_memoryTiles);
  if (_offlinePacks) heap_caps_free(_offlinePacks);
  _styles = nullptr;
  _places = nullptr;
  _memoryTiles = nullptr;
  _offlinePacks = nullptr;
  Tab5DebugSerial.println("[MAP] allocation PSRAM des tables impossible");
  return false;
}

void Tab5MapEngine::addDefaultStyles() {
  _styleCount = 0;
  auto add = [&](const char* id, const char* cacheId, const char* name,
                 const char* url,
                 const char* attribution, uint8_t maxZoom,
                 int8_t zoomShift, bool lightweight) {
    if (_styleCount >= TAB5_MAP_MAX_STYLES) return;
    MapStyle& style = _styles[_styleCount++];
    memset(&style, 0, sizeof(style));
    copyUtf8(style.id, sizeof(style.id), id);
    copyUtf8(style.cacheId, sizeof(style.cacheId), cacheId);
    copyUtf8(style.name, sizeof(style.name), name);
    copyUtf8(style.url, sizeof(style.url), url);
    copyUtf8(style.attribution, sizeof(style.attribution), attribution);
    style.maxZoom = static_cast<uint8_t>(
        constrain(static_cast<int>(maxZoom), 3, 22));
    style.zoomShift = static_cast<int8_t>(
        constrain(static_cast<int>(zoomShift), -2, 0));
    style.lightweight = lightweight;
  };
  add("osm", "osm", "OSM Standard",
      "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
      "© OpenStreetMap contributors", 19, 0, false);
  add("osm-fast", "osm", "OSM léger",
      "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
      "© OpenStreetMap contributors", 20, -1, true);
  add("opentopomap", "opentopomap", "OpenTopoMap",
      "https://tile.opentopomap.org/{z}/{x}/{y}.png",
      "© OpenStreetMap · SRTM · OpenTopoMap (CC-BY-SA)", 17, 0, false);
}

bool Tab5MapEngine::begin(bool sdAvailable, BaseType_t taskCore) {
  if (_ready) return true;
  if (!_lifecycleMutex)
    _lifecycleMutex = xSemaphoreCreateMutexStatic(&_lifecycleStorage);
  if (!_lifecycleMutex) return false;
  _taskCore = taskCore;
  if (!allocateRuntimeStorage()) return false;
  copyUtf8(_userAgent, sizeof(_userAgent),
           "Tab5OS/0.8.10 (M5Stack Tab5 map viewer)");
  copyUtf8(_searchUrl, sizeof(_searchUrl),
           "https://nominatim.openstreetmap.org/search");
  addDefaultStyles();
  _sdAvailable = sdAvailable;
  _storageLoaded = false;

  String requestedStyle = "osm";
  Preferences preferences;
  if (preferences.begin(MAP_PREFS_NAMESPACE, true)) {
    requestedStyle = preferences.getString("style", "osm");
    _importRateTenths=constrain(preferences.getUShort("importRate",5),1,40);
    _rememberView=preferences.getBool("remember",true);
    preferences.end();
  }
  for (size_t i = 0; i < _styleCount; ++i) {
    if (requestedStyle == _styles[i].id) {
      _selectedStyle = static_cast<uint8_t>(i);
      break;
    }
  }
  _ready = createQueues();
  return _ready;
}

bool Tab5MapEngine::createQueues() {
  if(_requestQueue&&_resultQueue&&_searchRequestQueue&&_searchResultQueue)return true;
  // These queues are task-only. Their data need no DMA/internal SRAM; keep
  // FreeRTOS control blocks internal and allocate only payloads in PSRAM.
  const size_t bytes=32*sizeof(TileRequest)+sizeof(Tab5MapTileResult)+
      2*sizeof(SearchRequest)+sizeof(Tab5MapSearchResult);
  auto* storage=static_cast<uint8_t*>(heap_caps_calloc(1,bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!storage)return false;
  _queueStorage=storage;
  _requestQueue=xQueueCreateStatic(32,sizeof(TileRequest),storage,&_queueControls[0]);
  storage+=32*sizeof(TileRequest);
  _resultQueue=xQueueCreateStatic(1,sizeof(Tab5MapTileResult),storage,&_queueControls[1]);
  storage+=sizeof(Tab5MapTileResult);
  _searchRequestQueue=xQueueCreateStatic(2,sizeof(SearchRequest),storage,&_queueControls[2]);
  storage+=2*sizeof(SearchRequest);
  _searchResultQueue=xQueueCreateStatic(1,sizeof(Tab5MapSearchResult),storage,&_queueControls[3]);
  if(_requestQueue&&_resultQueue&&_searchRequestQueue&&_searchResultQueue)return true;
  if(_requestQueue)vQueueDelete(_requestQueue);
  if(_resultQueue)vQueueDelete(_resultQueue);
  if(_searchRequestQueue)vQueueDelete(_searchRequestQueue);
  if(_searchResultQueue)vQueueDelete(_searchResultQueue);
  _requestQueue=_resultQueue=_searchRequestQueue=_searchResultQueue=nullptr;
  heap_caps_free(_queueStorage);_queueStorage=nullptr;
  return false;
}

void Tab5MapEngine::setStorageAvailable(bool available) {
  closeOfflinePack();
  _sdAvailable = available;
  _storageLoaded = false;
  _placesLoaded = false;
  _offlinePackCount = 0;
  _quarantineCount = _quarantineNext = 0;
  _placeCount = 0;++_localMarkerRevision;
  // RAM tiles can remain useful while the SD is temporarily unavailable.
}

void Tab5MapEngine::ensureStorageLoaded() {
  if (!_ready || !_sdAvailable || _storageLoaded) return;
  Tab5StorageUse storage{true};
  if (!storage) return;
  tab5FS.mkdir(MAP_ROOT);
  tab5FS.mkdir(MAP_CACHE_ROOT);
  tab5FS.mkdir(MAP_OFFLINE_ROOT);
  // Configuration is read once and copied under the network lifecycle lock.
  // Offline packs and places are UI-owned and refreshed after SD recovery.
  if (!_stylesConfigLoaded) {
    loadStylesConfig();
    _stylesConfigLoaded = true;
  }
  loadPlaces();
  loadOfflinePacks();
  _storageLoaded = true;
}

void Tab5MapEngine::setActive(bool active, uint32_t generation) {
  if (!_lifecycleMutex) return;
  if (active) {
    // Importers run while Cartes is hidden. Only replace its tables after the
    // old worker has published complete network cleanup.
    xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
    const bool reload = _sdAvailable && !_active && !_task;
    xSemaphoreGive(_lifecycleMutex);
    if (reload) {
      clearMemoryCache();
      closeOfflinePack();
      addDefaultStyles();
      _selectedStyle = 0;
      _stylesConfigLoaded = false;
      _storageLoaded = false;
    }
    ensureStorageLoaded();
  }
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  _generation = generation;
  _active = active;
  if (!active) _searchRequestId = 0;
  if (!active && _task) xTaskNotifyGive(_task);
  xSemaphoreGive(_lifecycleMutex);
  if (!active) {
    if (_requestQueue) xQueueReset(_requestQueue);
    discardSearchRequests();
    closeOfflinePack();
    Tab5MapTileResult tile = {};
    while (poll(tile)) release(tile);
    Tab5MapSearchResult search = {};
    while (pollSearch(search)) releaseSearch(search);
    // readCachedTile returns an owned copy; no renderer borrows these entries.
    // Keep the on-SD cache and view/preferences, release the app's RAM copies
    // as soon as it is left, before another app requests memory.
    clearMemoryCache();
  }
}

void Tab5MapEngine::beginGeneration(uint32_t generation) {
  if (!_lifecycleMutex) return;
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  _generation = generation;
  if (_requestQueue) xQueueReset(_requestQueue);
  if (_task) xTaskNotifyGive(_task);
  xSemaphoreGive(_lifecycleMutex);
  Tab5MapTileResult stale;
  while (poll(stale)) release(stale);
}

bool Tab5MapEngine::requestTile(uint8_t zoom, uint32_t x, uint32_t y,
                                uint32_t generation, bool regionImport) {
  if (!_ready || !_active || !_requestQueue || generation != _generation ||
      _selectedStyle >= _styleCount) return false;
  if (zoom > Tab5MapViewPolicy::nativeMax(styleMaxZoom(_selectedStyle),styleZoomShift(_selectedStyle)) ||
      x >= (1UL << zoom) || y >= (1UL << zoom)) return false;
  if (regionImport && !styleImportAllowed(_selectedStyle)) return false;
  if (!ensureTask()) return false;
  const TileRequest request = {zoom, _selectedStyle, x, y, generation, regionImport};
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  const bool sent = _active && generation == _generation && _task &&
      xQueueSend(_requestQueue, &request, 0) == pdTRUE;
  if (sent) xTaskNotifyGive(_task);
  xSemaphoreGive(_lifecycleMutex);
  return sent;
}

bool Tab5MapEngine::styleImportAllowed(size_t index) const {
  return index<_styleCount && Tab5MapImport::allowed(_styles[index].url);
}
uint16_t Tab5MapEngine::importEffectiveRateTenths(size_t index,uint16_t preview) const {
  return Tab5MapImport::effectiveRate(preview?preview:_importRateTenths.load(),index<_styleCount?_styles[index].url:"");
}
bool Tab5MapEngine::setImportRateTenths(uint16_t value) {
  value=constrain(value,1,40);_importRateTenths=value;
  Preferences p;if(!p.begin(MAP_PREFS_NAMESPACE,false))return false;
  const bool saved=p.putUShort("importRate",value)==sizeof(value);p.end();return saved;
}


bool Tab5MapEngine::poll(Tab5MapTileResult& result) {
  if (!_resultQueue) return false;
  return xQueueReceive(_resultQueue, &result, 0) == pdTRUE;
}

void Tab5MapEngine::release(Tab5MapTileResult& result) {
  if (result.data) heap_caps_free(result.data);
  result.data = nullptr;
  result.size = 0;
}

void Tab5MapEngine::discardSearchRequests() {
  if(!_searchRequestQueue)return;
  SearchRequest stale{};
  while(xQueueReceive(_searchRequestQueue,&stale,0)==pdTRUE)heap_caps_free(stale.points);
}

bool Tab5MapEngine::requestSearch(const String& rawQuery,uint32_t requestId,
                                  double latitude,double longitude,bool online) {
  if(!_ready||!_active||!_searchRequestQueue||!requestId)return false;
  String query=rawQuery;query.trim();
  if(query.length()<2||query.length()>=sizeof(SearchRequest::query)||
     !Tab5GeoIndex::validCoordinate(latitude,longitude))return false;
  if(!ensureTask())return false;
  SearchRequest request{};
  String language=Tab5Locale.selectedId();
  bool validLanguage=language.length()&&language.length()<sizeof(request.language);
  for(size_t i=0;validLanguage&&i<language.length();++i){const char c=language[i];validLanguage=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-';}
  copyUtf8(request.language,sizeof(request.language),validLanguage?language:String("en-US"));
  copyUtf8(request.query,sizeof(request.query),query);request.requestId=requestId;
  request.latitude=latitude;request.longitude=longitude;
  request.online=online;
  // Capture UI-owned favorites and Mesh for both modes: Wi-Fi may disappear
  // while an explicit online request waits, and local fallback must be complete.
  {
    const size_t count=std::min<size_t>(markerCount(),TAB5_MAP_MAX_PLACES+33);
    if(count){
      request.points=static_cast<Tab5MapMarker*>(heap_caps_calloc(count,sizeof(Tab5MapMarker),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
      if(!request.points)return false;
      for(size_t i=0;i<count;++i){Tab5MapMarker point;if(marker(i,point))request.points[request.pointCount++]=point;}
    }
  }
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  _searchRequestId = requestId;
  discardSearchRequests();
  const bool sent = _active && _task &&
      xQueueSend(_searchRequestQueue, &request, 0) == pdTRUE;
  if (sent) xTaskNotifyGive(_task);
  xSemaphoreGive(_lifecycleMutex);
  if (!sent) heap_caps_free(request.points);
  return sent;
}

bool Tab5MapEngine::pollSearch(Tab5MapSearchResult& result) {
  if (!_searchResultQueue) return false;
  return xQueueReceive(_searchResultQueue, &result, 0) == pdTRUE;
}

void Tab5MapEngine::releaseSearch(Tab5MapSearchResult& result) {
  if (result.items) heap_caps_free(result.items);
  result.items = nullptr;
  result.count = 0;
}

size_t Tab5MapEngine::queuedRequests() const {
  return _requestQueue ? uxQueueMessagesWaiting(_requestQueue) : 0;
}

void Tab5MapEngine::taskEntry(void* parameter) {
  auto* self = static_cast<Tab5MapEngine*>(parameter);
  for (;;) {
    self->taskLoop();
    // taskLoop's socket objects have been destroyed before publication. If
    // the UI reactivated Cartes during closure, this worker consumes the new
    // generation instead of exiting underneath an already queued request.
    xSemaphoreTake(self->_lifecycleMutex, portMAX_DELAY);
    const bool canExit = !self->_active;
    if (canExit) self->_task = nullptr;
    xSemaphoreGive(self->_lifecycleMutex);
    if (canExit) break;
  }
  tab5TaskCheckpoint("tab5_map_net");
  vTaskDelete(nullptr);
}

bool Tab5MapEngine::ensureTask() {
  if (!_lifecycleMutex) return false;
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  if (_task) { xSemaphoreGive(_lifecycleMutex); return true; }
  // Reclaimers run only on the UI task and never acquire this lifecycle lock.
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Map,
      Tab5Resources::TaskBudget(MAP_TASK_STACK),"tache cartes");
  if(!admission){xSemaphoreGive(_lifecycleMutex);return false;}
  TaskHandle_t task = nullptr;
  const BaseType_t created = tab5CreateAdmittedTask(admission,
      taskEntry, "tab5_map_net", MAP_TASK_STACK, this, MAP_TASK_PRIORITY,
      &task, _taskCore);
  if (created == pdPASS) _task = task;
  xSemaphoreGive(_lifecycleMutex);
  return created == pdPASS;
}

void Tab5MapEngine::taskLoop() {
  Tab5MapConnection connection;
  uint32_t lastSearchMs = 0;
  TileRequest deferredTile{};bool hasDeferredTile=false;
  while (true) {
    if (!_active) {
      connection.close();
      xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
      const bool canExit = !_active;
      xSemaphoreGive(_lifecycleMutex);
      if (canExit) return;
    }
    if(!_active||tab5NetworkBackgroundYieldRequested()||millis()-connection.used>1500)connection.close();
    SearchRequest search = {};
    if (xQueueReceive(_searchRequestQueue, &search, 0) == pdTRUE) {
      connection.close();
      if(!_active){heap_caps_free(search.points);continue;}
      const auto networkReady=[](){return tab5NetworkEnabled()&&WiFi.status()==WL_CONNECTED;};
      if (search.online) {
        // A connected station is not proof that the Internet is reachable.
        // Prefer an online lookup when its shared transport is available, then
        // fall back locally on failure/empty results. Never wait indefinitely
        // for another connection setup just to search local data.
        search.online = networkReady() && !tab5NetworkBackgroundYieldRequested();
        while (search.online && lastSearchMs && millis() - lastSearchMs < 1100 &&
               _active && search.requestId == _searchRequestId &&
               networkReady() && !tab5NetworkBackgroundYieldRequested())
          vTaskDelay(pdMS_TO_TICKS(25));
        search.online = search.online && networkReady() &&
            !tab5NetworkBackgroundYieldRequested();
        if (search.online && _active && search.requestId == _searchRequestId)
          lastSearchMs = millis();
      }
      Tab5MapSearchResult result{};result.requestId=search.requestId;
      if (_active && search.requestId == _searchRequestId) {
        result.success = search.online ? downloadSearch(search, result, connection)
                                       : localSearch(search, result);
        if (search.online && (!result.success || !result.count) &&
            _active && search.requestId == _searchRequestId) {
          // The current request still owns its captured favorites/Mesh points.
          // Discard partial online allocations before using the SD index, and
          // do not turn an obsolete/cancelled search into fresh local I/O.
          Tab5DebugSerial.printf("[MAP][SEARCH] fallback HTTP=%d TLS=%d results=%u\n",
              result.httpCode, result.tlsCode, unsigned(result.count));
          connection.close();
          releaseSearch(result);
          result = {};
          result.requestId = search.requestId;
          result.success = localSearch(search, result);
        }
      }
      heap_caps_free(search.points);search.points=nullptr;connection.close();
      bool sent=false;
      while(!sent){
        xSemaphoreTake(_lifecycleMutex,portMAX_DELAY);
        const bool current=_active&&search.requestId==_searchRequestId;
        if(current)sent=xQueueSend(_searchResultQueue,&result,0)==pdTRUE;
        xSemaphoreGive(_lifecycleMutex);
        if(!current)break;
        if(!sent)vTaskDelay(pdMS_TO_TICKS(20));
      }
      if(!sent)releaseSearch(result);
      continue;
    }

    TileRequest request = {};
    const bool received=hasDeferredTile?(request=deferredTile,hasDeferredTile=false,true):xQueueReceive(_requestQueue,&request,0)==pdTRUE;
    if (received) {
      if (!_active || request.generation != _generation)
        continue;
      // Connection setup and driver changes request a short yield. An already
      // established radio can coexist with this single tile request.
      while (_active && request.generation == _generation &&
             tab5NetworkBackgroundYieldRequested() && tab5NetworkEnabled()) {
        // A queued offline lookup must remain available during audio streaming.
        // Keep the tile locally; requeueing into a full queue could lose it.
        if(uxQueueMessagesWaiting(_searchRequestQueue)){deferredTile=request;hasDeferredTile=true;break;}
        vTaskDelay(pdMS_TO_TICKS(50));
      }
      if(hasDeferredTile)continue;
      if (!_active || request.generation != _generation)
        continue;
      Tab5MapTileResult result = {};
      result.zoom = request.zoom;
      result.style = request.style;
      result.x = request.x;
      result.y = request.y;
      result.generation = request.generation;
      // Region imports pause on the first error. In particular 403/429 never
      // trigger an automatic retry; completed SD tiles make explicit resume safe.
      if(request.regionImport){result.attempts=1;result.success=downloadTile(request,result,connection);}
      else result.success=tab5tile::downloadWithRetries(result.httpCode,result.attempts,
        [&](){return downloadTile(request,result,connection);},
        [&](){return _active&&request.generation==_generation;},
        [&](unsigned ms){const uint32_t start=millis();while(_active&&request.generation==_generation&&millis()-start<ms)vTaskDelay(pdMS_TO_TICKS(25));},
        [&](unsigned attempt,int code){Tab5DebugSerial.printf("[MAP][RETRY] z%u/%lu/%lu tentative %u/%u HTTP=%d\n",request.zoom,(unsigned long)request.x,(unsigned long)request.y,attempt,tab5tile::MaxAttempts,code);});
      bool sent = false;
      while (!sent) {
        if (tab5NetworkBackgroundYieldRequested()||millis()-connection.used>1500) connection.close();
        xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
        const bool current = _active && (request.generation == _generation);
        if (current) sent = xQueueSend(_resultQueue, &result, 0) == pdTRUE;
        xSemaphoreGive(_lifecycleMutex);
        if (!current) break;
        if (!sent) vTaskDelay(pdMS_TO_TICKS(20));
      }
      if (!sent) release(result);
      continue;
    }
    // Both request queues publish a task notification while holding the same
    // lifecycle lock used at exit. A notification before this wait is retained.
    // Only a live keep-alive socket needs bounded expiry/foreground-yield checks.
    ulTaskNotifyTake(pdTRUE, connection.retained() ? pdMS_TO_TICKS(250) : portMAX_DELAY);
  }
}

bool Tab5MapEngine::downloadTile(const TileRequest& request,
                                 Tab5MapTileResult& result, Tab5MapConnection& connection) {
  if (WiFi.status() != WL_CONNECTED || request.style >= _styleCount) {
    result.httpCode = -100;
    return false;
  }

  if(request.regionImport){
    if(!styleImportAllowed(request.style)){result.httpCode=403;return false;}
    const uint32_t gap=Tab5MapImport::interval(importEffectiveRateTenths(request.style));
    const bool ready=Tab5MapImport::waitForSlot(_haveImportRequest,_lastImportRequestMs,gap,
        [&](){return _active&&request.generation==_generation&&tab5NetworkEnabled();},
        [](){return millis();},[&](){
          if(tab5NetworkBackgroundYieldRequested()||millis()-connection.used>1500)connection.close();
          vTaskDelay(pdMS_TO_TICKS(20));
        });
    if(!ready){connection.close();result.httpCode=-107;return false;}
  }

  const String url = expandedUrl(request.style, request.zoom,
                                 request.x, request.y);
  struct Cancellation {
    Tab5MapEngine* engine;
    const TileRequest* request;
  } cancellation{this, &request};
  const auto cancelled = [](void* context) {
    const auto* c = static_cast<Cancellation*>(context);
    return !tab5NetworkEnabled() || tab5NetworkBackgroundYieldRequested() ||
        !c->engine->_active || c->request->generation != c->engine->_generation;
  };
  Tab5NetworkLease lease("cartes", 20000, cancelled, &cancellation, true);
  if (!lease) { result.httpCode = -107; return false; }
  Tab5MapRequestCancellation cancellationBinding(connection, cancelled, &cancellation);
  if(cancelled(&cancellation)){connection.close();result.httpCode=-107;return false;}
  const bool reused=connection.reuse(url);
  Tab5Resources::Reservation admission;
  if(!reused){admission=tab5ReserveMemory(Tab5Resources::Owner::Map,Tab5Resources::tlsBudget(),"TLS cartes");
    if(!admission){result.httpCode=-109;return false;}}
  WiFi.setSleep(false);tab5NetDiagnosticsService();Tab5MapNetworkDiagnosticGuard networkDiagnostic;
  auto& secureClient=connection.secure;NetworkClient* client=&connection.client();auto& http=connection.http;
  http.setConnectTimeout(MAP_HTTP_TIMEOUT_MS);
  http.setTimeout(MAP_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  http.setUserAgent(_userAgent);
  xSemaphoreGive(_lifecycleMutex);
  http.setReuse(true);
  if (!http.begin(*client, url)) {
    result.httpCode = -101;
    return false;
  }
  http.addHeader("Accept", "image/png");
  if(request.regionImport){_lastImportRequestMs=millis();_haveImportRequest=true;}
  result.httpCode = http.GET();
  // The live TLS allocation is already reflected in the free-heap sample.
  // Keeping its startup reservation would count it twice during raster work.
  admission.release();
  if(cancelled(&cancellation)){connection.close();result.httpCode=-107;return false;}
  if (result.httpCode != HTTP_CODE_OK) {
    char tlsMessage[96] = {};
    result.tlsCode = url.startsWith("https://")
        ? secureClient.lastError(tlsMessage, sizeof(tlsMessage)) : 0;
    if (result.httpCode < 0 && mapConnectionFailureLogDue()) {
      const int tlsCode = result.tlsCode;
      const int rssi = Tab5Audio.wifiRssi();
      char signalText[20];
      if (rssi < 0) snprintf(signalText, sizeof(signalText), "%d dBm", rssi);
      else snprintf(signalText, sizeof(signalText), "n/a");
      Tab5DebugSerial.printf("[MAP][NET] GET impossible : HTTP=%d (%s), TLS=%d (%s), "
                    "WiFi=%d, RSSI(cache)=%s, RAM=%u Kio, bloc=%u Kio\n",
                    result.httpCode,
                    HTTPClient::errorToString(result.httpCode).c_str(),
                    tlsCode, tlsMessage[0] ? tlsMessage : "n/a",
                    static_cast<int>(WiFi.status()), signalText,
                    static_cast<unsigned>(heap_caps_get_free_size(
                        MALLOC_CAP_INTERNAL) / 1024U),
                    static_cast<unsigned>(heap_caps_get_largest_free_block(
                        MALLOC_CAP_INTERNAL) / 1024U));
      Tab5DebugSerial.flush();
    }
    connection.close();
    return false;
  }
  const int contentLength = http.getSize();
  if (contentLength > static_cast<int>(MAX_TILE_BYTES)) {
    result.httpCode = -102;
    connection.close();
    return false;
  }

  PsramBufferStream sink(MAX_TILE_BYTES,
      contentLength > 0 ? static_cast<size_t>(contentLength) : 0,
      cancelled, &cancellation);
  if (!sink.valid()) {
    result.httpCode = -103;
    connection.close();
    return false;
  }
  const int written = http.writeToStream(&sink);
  if(written>0&&sink.valid()&&!sink.overflowed()&&!sink.cancelled()&&(contentLength<=0||sink.size()==size_t(contentLength)))http.end();
  else connection.close();
  if (sink.cancelled()) { connection.close();result.httpCode = -107; return false; }
  if (!sink.valid()) { result.httpCode = -103; return false; }
  if (sink.overflowed()) { result.httpCode = -102; return false; }
  if (written <= 0 || !sink.size() ||
      (contentLength > 0 && sink.size() != static_cast<size_t>(contentLength))) {
    result.httpCode = written < 0 ? written : -104;
    return false;
  }
  uint8_t* data = sink.detach();
  const auto validation = Tab5TileValidation::validate(data, sink.size());
  if (validation != Tab5TileValidation::Result::Valid) {
    heap_caps_free(data);
    result.httpCode = validation == Tab5TileValidation::Result::NoMemory ? -103 :
                      validation == Tab5TileValidation::Result::Unavailable ? -107 : -105;
    return false;
  }
  if(cancelled(&cancellation)){heap_caps_free(data);connection.close();result.httpCode=-107;return false;}
  result.data = data;
  result.size = sink.size();
  return true;
}

bool Tab5MapEngine::downloadSearch(const SearchRequest& request,
                                   Tab5MapSearchResult& result, Tab5MapConnection& connection) {
  struct Cancellation { Tab5MapEngine* engine; uint32_t requestId; } cancellation{this, request.requestId};
  const auto cancelled = [](void* context) {
    auto* c = static_cast<Cancellation*>(context);
    return !tab5NetworkEnabled() || tab5NetworkBackgroundYieldRequested() ||
        !c->engine->_active || c->requestId != c->engine->_searchRequestId;
  };
  Tab5NetworkLease lease("recherche", 20000, cancelled, &cancellation, true);
  if (!lease) { result.httpCode = -107; return false; }
  Tab5MapRequestCancellation cancellationBinding(connection, cancelled, &cancellation);
  if (WiFi.status() != WL_CONNECTED) {
    result.httpCode = -100;
    return false;
  }
  String url(_searchUrl);
  url += url.indexOf('?') >= 0 ? '&' : '?';
  url += "format=jsonv2&addressdetails=0&namedetails=1&limit=";
  url += String(TAB5_MAP_MAX_SEARCH_RESULTS);
  url += "&q=";
  url += urlEncoded(request.query);

  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Map,Tab5Resources::tlsBudget(),"recherche cartes");
  if(!admission){result.httpCode=-109;return false;}
  WiFi.setSleep(false);
  tab5NetDiagnosticsService();
  Tab5MapNetworkDiagnosticGuard networkDiagnostic;
  // The worker already owns these socket objects. Reuse that allocation and
  // release TLS before parsing instead of constructing a second TLS context.
  connection.reuse(url);
  auto& http = connection.http;
  http.setConnectTimeout(MAP_HTTP_TIMEOUT_MS);
  http.setTimeout(MAP_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  http.setUserAgent(_userAgent);
  xSemaphoreGive(_lifecycleMutex);
  http.setReuse(false);
  if (!http.begin(connection.client(), url)) {
    connection.close();
    result.httpCode = -101;
    return false;
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("Accept-Language", request.language);
  result.httpCode = http.GET();
  admission.release();
  if (cancelled(&cancellation)) {
    connection.close(); result.httpCode = -107; return false;
  }
  if (result.httpCode != HTTP_CODE_OK) {
    char tlsMessage[96] = {};
    result.tlsCode = url.startsWith("https://")
        ? connection.secure.lastError(tlsMessage, sizeof(tlsMessage)) : 0;
    if (result.httpCode < 0 && mapConnectionFailureLogDue()) {
      const int tlsCode = result.tlsCode;
      const int rssi = Tab5Audio.wifiRssi();
      char signalText[20];
      if (rssi < 0) snprintf(signalText, sizeof(signalText), "%d dBm", rssi);
      else snprintf(signalText, sizeof(signalText), "n/a");
      Tab5DebugSerial.printf("[MAP][NET] recherche impossible : HTTP=%d (%s), "
                    "TLS=%d (%s), WiFi=%d, RSSI(cache)=%s\n",
                    result.httpCode,
                    HTTPClient::errorToString(result.httpCode).c_str(),
                    tlsCode, tlsMessage[0] ? tlsMessage : "n/a",
                    static_cast<int>(WiFi.status()), signalText);
      Tab5DebugSerial.flush();
    }
    connection.close();
    return false;
  }
  const int contentLength = http.getSize();
  if (contentLength > static_cast<int>(MAX_SEARCH_BYTES)) {
    result.httpCode = -102;
    connection.close();
    return false;
  }
  PsramBufferStream sink(MAX_SEARCH_BYTES,
      contentLength > 0 ? static_cast<size_t>(contentLength) : 0,
      cancelled, &cancellation);
  if (!sink.valid()) {
    result.httpCode = -103;
    connection.close();
    return false;
  }
  const int written = http.writeToStream(&sink);
  connection.close();
  if (sink.cancelled() || cancelled(&cancellation)) { result.httpCode = -107; return false; }
  if (!sink.valid()) { result.httpCode = -103; return false; }
  if (sink.overflowed()) { result.httpCode = -102; return false; }
  if (written <= 0 || !sink.size() ||
      (contentLength > 0 && sink.size() != static_cast<size_t>(contentLength))) {
    result.httpCode = written < 0 ? written : -104;
    return false;
  }

  auto* items = static_cast<Tab5MapSearchItem*>(heap_caps_calloc(
      TAB5_MAP_MAX_SEARCH_RESULTS, sizeof(Tab5MapSearchItem),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!items) {
    result.httpCode = -103;
    return false;
  }

  const int count = Tab5MapSearch::decode(sink.data(), sink.size(), request.language,
      items, TAB5_MAP_MAX_SEARCH_RESULTS, tab5JsonAllocator());
  if (count < 0 || cancelled(&cancellation)) {
    heap_caps_free(items);
    result.httpCode = cancelled(&cancellation) ? -107 : count == -2 ? -103 : -106;
    return false;
  }
  result.items = items;
  result.count = count;
  result.success = true;
  return true;
}

bool Tab5MapEngine::loadStylesConfig() {
  {
    File file = tab5FS.open(MAP_STYLES_FILE, FILE_READ);
    if (file && !file.isDirectory() && file.size() <= MAX_CONFIG_BYTES) {
      JsonDocument document(tab5JsonAllocator());
      if (!deserializeJson(document, file)) {
        const String agent = String(document["user_agent"] | "");
        if (agent.length() && !agent.startsWith("Tab5OS/")) {
          xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
          copyUtf8(_userAgent, sizeof(_userAgent), agent);
          xSemaphoreGive(_lifecycleMutex);
        }
      }
      file.close();
    }
  }
  File file = tab5FS.open("/System/map_catalog.json", FILE_READ);
  JsonDocument document(tab5JsonAllocator());
  if (file && !file.isDirectory() && file.size() <= 16384) {
    if (deserializeJson(document, file)) document.clear();
  }
  if (file) file.close();
  auto validId = [](const String& value) {
    if (!value.length() || value.length() > 23) return false;
    for (unsigned i = 0; i < value.length(); ++i) {
      const char c = value[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
  };
  xSemaphoreTake(_lifecycleMutex, portMAX_DELAY);
  for (JsonObjectConst item : document["styles"].as<JsonArrayConst>()) {
    const String id = String(item["id"] | "");
    if (id.startsWith("ign-") || id.startsWith("tf-")) continue;
    const String cacheId = String(item["cache_id"] | "");
    const String name = String(item["name"] | "");
    const String url = String(item["url"] | "");
    const String attribution = String(item["attribution"] | "");
    const int low = item["min_zoom"] | 0;
    const int high = item["max_zoom"] | 19;
    if (!validId(id) || !validId(cacheId) || !name.length() || name.length() >= 48 ||
        !attribution.length() || attribution.length() >= 128 || url.length() >= 512 ||
        !url.startsWith("https://") || url.indexOf("{z}") < 0 ||
        url.indexOf("{x}") < 0 || url.indexOf("{y}") < 0 ||
        low < 0 || high < low || high > 22) continue;
    bool valid = true;
    for (unsigned i = 0; i < url.length(); ++i)
      if (uint8_t(url[i]) <= 32) { valid = false; break; }
    size_t target = _styleCount;
    for (size_t i = 0; i < _styleCount; ++i) {
      if (id == _styles[i].id) {
        // A provider imported by the downloader may replace its own endpoint.
        if (cacheId != _styles[i].cacheId) valid = false;
        target = i;
        break;
      }
    }
    if (target == _styleCount) {
      for (size_t i = 0; i < _styleCount; ++i)
        if (cacheId == _styles[i].cacheId) valid = false;
    }
    if (!valid || target >= TAB5_MAP_MAX_STYLES) continue;
    const bool replacement = target < _styleCount;
    const bool lightweight = replacement && _styles[target].lightweight;
    const int8_t zoomShift = replacement ? _styles[target].zoomShift : 0;
    if (!replacement) ++_styleCount;
    MapStyle& style = _styles[target];
    memset(&style, 0, sizeof(style));
    style.lightweight = lightweight;
    style.zoomShift = zoomShift;
    copyUtf8(style.id, sizeof(style.id), id);
    copyUtf8(style.cacheId, sizeof(style.cacheId), cacheId);
    copyUtf8(style.name, sizeof(style.name), name);
    copyUtf8(style.url, sizeof(style.url), url);
    copyUtf8(style.attribution, sizeof(style.attribution), attribution);
    style.minZoom = low; style.maxZoom = high;
  }
  xSemaphoreGive(_lifecycleMutex);
  Preferences preferences;
  if (preferences.begin(MAP_PREFS_NAMESPACE, true)) {
    const String requestedStyle = preferences.getString("style", "osm");
    preferences.end();
    for (size_t i = 0; i < _styleCount; ++i)
      if (requestedStyle == _styles[i].id) { _selectedStyle = i; break; }
  }
  return true;
}


String Tab5MapEngine::expandedUrl(uint8_t style, uint8_t zoom, uint32_t x,
                                  uint32_t y) const {
  if (style >= _styleCount) return String();
  String url(_styles[style].url);
  url.replace("{z}", String(zoom));
  url.replace("{x}", String(x));
  url.replace("{y}", String(y));
  return url;
}

uint64_t Tab5MapEngine::offlineTileKey(uint8_t zoom, uint32_t x,
                                       uint32_t y) {
  return (static_cast<uint64_t>(zoom) << 58) |
         (static_cast<uint64_t>(x) << 29) |
         static_cast<uint64_t>(y);
}

bool Tab5MapEngine::readOfflineHeader(const char* path, OfflinePack& pack) {
  if (!path || !path[0]) return false;
  File file = tab5FS.open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    return false;
  }
  const uint64_t fileSize = file.size();
  T5MapFileHeader header = {};
  const size_t read = file.read(reinterpret_cast<uint8_t*>(&header),
                                sizeof(header));
  file.close();
  if (read != sizeof(header) ||
      memcmp(header.magic, T5MAP_MAGIC, sizeof(T5MAP_MAGIC)) != 0 ||
      header.version != T5MAP_VERSION ||
      header.headerSize != T5MAP_HEADER_SIZE || !header.entryCount ||
      !header.directoryCount || header.entriesPerBlock < 64 ||
      header.entriesPerBlock > 4096 || header.minKey > header.maxKey) {
    return false;
  }
  header.cacheId[sizeof(header.cacheId) - 1] = 0;
  header.name[sizeof(header.name) - 1] = 0;
  if (!safeIdentifier(String(header.cacheId))) return false;
  const uint32_t expectedDirectories =
      (header.entryCount + header.entriesPerBlock - 1U) /
      header.entriesPerBlock;
  const uint64_t directoryEnd = static_cast<uint64_t>(header.directoryOffset) +
      static_cast<uint64_t>(header.directoryCount) *
      sizeof(OfflineDirectoryEntry);
  const uint64_t indexEnd = static_cast<uint64_t>(header.indexOffset) +
      static_cast<uint64_t>(header.entryCount) * sizeof(OfflineIndexEntry);
  if (header.directoryCount != expectedDirectories ||
      header.directoryOffset < T5MAP_HEADER_SIZE ||
      directoryEnd > header.indexOffset || indexEnd > header.dataOffset ||
      header.dataOffset >= fileSize || fileSize > UINT32_MAX) {
    return false;
  }
  memset(&pack, 0, sizeof(pack));
  copyUtf8(pack.path, sizeof(pack.path), path);
  copyUtf8(pack.cacheId, sizeof(pack.cacheId), header.cacheId);
  copyUtf8(pack.name, sizeof(pack.name),
           header.name[0] ? String(header.name) : String(header.cacheId));
  pack.entryCount = header.entryCount;
  pack.directoryCount = header.directoryCount;
  pack.entriesPerBlock = header.entriesPerBlock;
  pack.directoryOffset = header.directoryOffset;
  pack.indexOffset = header.indexOffset;
  pack.dataOffset = header.dataOffset;
  pack.minKey = header.minKey;
  pack.maxKey = header.maxKey;
  return true;
}

bool Tab5MapEngine::loadOfflinePacks() {
  closeOfflinePack();
  _offlinePackCount = 0;
  _quarantineCount = _quarantineNext = 0;
  memset(_offlinePacks, 0,
         TAB5_MAP_MAX_OFFLINE_PACKS * sizeof(OfflinePack));
  tab5FS.mkdir(MAP_OFFLINE_ROOT);
  File directory = tab5FS.open(MAP_OFFLINE_ROOT, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return false;
  }
  for (File entry = directory.openNextFile(); entry &&
       _offlinePackCount < TAB5_MAP_MAX_OFFLINE_PACKS;
       entry = directory.openNextFile()) {
    const String path = entry.path();
    const bool regular = !entry.isDirectory();
    entry.close();
    String lower = path;
    lower.toLowerCase();
    if (!regular || !lower.endsWith(".t5map")) continue;
    OfflinePack pack = {};
    if (!readOfflineHeader(path.c_str(), pack)) {
      Tab5DebugSerial.printf("[MAP] paquet hors ligne ignoré : %s\n", path.c_str());
      continue;
    }
    _offlinePacks[_offlinePackCount++] = pack;
    Tab5DebugSerial.printf("[MAP] paquet SD %s : %lu tuiles (%s)\n",
                  pack.name, static_cast<unsigned long>(pack.entryCount),
                  pack.cacheId);
  }
  directory.close();

  // Les volumes générés sont des plages de clés. Les trier rend leur recherche
  // déterministe, quel que soit l'ordre retourné par FAT.
  for (size_t i = 1; i < _offlinePackCount; ++i) {
    OfflinePack value = _offlinePacks[i];
    size_t j = i;
    while (j > 0) {
      const int cacheOrder = strcmp(_offlinePacks[j - 1].cacheId,
                                    value.cacheId);
      if (cacheOrder < 0 ||
          (cacheOrder == 0 && _offlinePacks[j - 1].minKey <= value.minKey))
        break;
      _offlinePacks[j] = _offlinePacks[j - 1];
      --j;
    }
    _offlinePacks[j] = value;
  }
  return _offlinePackCount > 0;
}

void Tab5MapEngine::closeOfflinePack() {
  // A false File may still own an invalidated implementation and volume
  // reference. Release ownership regardless of its current readable state.
  _offlineFile.close();
  if (_offlineDirectory) heap_caps_free(_offlineDirectory);
  if (_offlineIndexBlock) heap_caps_free(_offlineIndexBlock);
  _offlineDirectory = nullptr;
  _offlineIndexBlock = nullptr;
  _offlineIndexCapacity = 0;
  _offlineActivePack = -1;
  _offlineActiveBlock = UINT32_MAX;
}

bool Tab5MapEngine::activateOfflinePack(size_t index) {
  static_assert(sizeof(OfflineDirectoryEntry) == 16,
                "Entrée de répertoire T5MAP inattendue");
  static_assert(sizeof(OfflineIndexEntry) == 16,
                "Entrée d'index T5MAP inattendue");
  if (index >= _offlinePackCount) return false;
  if (_offlineActivePack == static_cast<int8_t>(index) && _offlineFile &&
      _offlineDirectory && _offlineIndexBlock) return true;
  closeOfflinePack();
  const OfflinePack& pack = _offlinePacks[index];
  const size_t directoryBytes = static_cast<size_t>(pack.directoryCount) *
      sizeof(OfflineDirectoryEntry);
  const size_t indexBytes = static_cast<size_t>(pack.entriesPerBlock) *
      sizeof(OfflineIndexEntry);
  if (!directoryBytes || directoryBytes > MAP_MAX_OFFLINE_DIRECTORY ||
      !indexBytes) return false;
  _offlineDirectory = static_cast<OfflineDirectoryEntry*>(heap_caps_malloc(
      directoryBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  _offlineIndexBlock = static_cast<OfflineIndexEntry*>(heap_caps_malloc(
      indexBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!_offlineDirectory || !_offlineIndexBlock) {
    closeOfflinePack();
    return false;
  }
  _offlineFile = tab5FS.open(pack.path, FILE_READ);
  if (!_offlineFile || !_offlineFile.seek(pack.directoryOffset) ||
      !readFileCooperatively(
          _offlineFile, reinterpret_cast<uint8_t*>(_offlineDirectory),
          directoryBytes)) {
    closeOfflinePack();
    return false;
  }
  uint64_t previousKey = 0;
  for (uint32_t i = 0; i < pack.directoryCount; ++i) {
    const OfflineDirectoryEntry& item = _offlineDirectory[i];
    if (!item.entryCount || item.entryCount > pack.entriesPerBlock ||
        item.firstEntry >= pack.entryCount ||
        static_cast<uint64_t>(item.firstEntry) + item.entryCount >
            pack.entryCount ||
        (i && item.firstKey <= previousKey)) {
      closeOfflinePack();
      return false;
    }
    previousKey = item.firstKey;
  }
  _offlineIndexCapacity = pack.entriesPerBlock;
  _offlineActivePack = static_cast<int8_t>(index);
  _offlineActiveBlock = UINT32_MAX;
  return true;
}

bool Tab5MapEngine::quarantinedOffline(size_t pack, uint64_t key) const {
  for (unsigned i=0;i<_quarantineCount;++i)
    if (_quarantinedTiles[i].pack==pack && _quarantinedTiles[i].key==key) return true;
  return false;
}
void Tab5MapEngine::quarantineOffline(size_t pack, uint64_t key) {
  if (quarantinedOffline(pack,key)) return;
  if (!_quarantinedTiles) _quarantinedTiles=static_cast<QuarantinedTile*>(
      heap_caps_malloc(64*sizeof(QuarantinedTile),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if (!_quarantinedTiles) return;
  _quarantinedTiles[_quarantineNext]={key,uint16_t(pack)};
  _quarantineNext=(_quarantineNext+1)%64;
  if (_quarantineCount<64) ++_quarantineCount;
}

bool Tab5MapEngine::readOfflineTile(uint8_t style, uint8_t zoom, uint32_t x,
                                    uint32_t y, uint8_t*& destination,
                                    size_t& size) {
  size = 0;
  if (style >= _styleCount) return false;
  const uint64_t key = offlineTileKey(zoom, x, y);
  for (size_t packIndex = 0; packIndex < _offlinePackCount; ++packIndex) {
    const OfflinePack& pack = _offlinePacks[packIndex];
    if (strcmp(pack.cacheId, _styles[style].cacheId) != 0 ||
        key < pack.minKey || key > pack.maxKey || quarantinedOffline(packIndex,key)) continue;
    if (!activateOfflinePack(packIndex)) continue;

    uint32_t low = 0;
    uint32_t high = pack.directoryCount;
    while (low < high) {
      const uint32_t middle = low + (high - low) / 2U;
      if (_offlineDirectory[middle].firstKey <= key) low = middle + 1U;
      else high = middle;
    }
    if (!low) continue;
    const uint32_t block = low - 1U;
    const OfflineDirectoryEntry& directory = _offlineDirectory[block];
    if (_offlineActiveBlock != block) {
      const uint64_t indexPosition = static_cast<uint64_t>(pack.indexOffset) +
          static_cast<uint64_t>(directory.firstEntry) *
          sizeof(OfflineIndexEntry);
      const size_t indexBytes = static_cast<size_t>(directory.entryCount) *
          sizeof(OfflineIndexEntry);
      if (indexPosition > UINT32_MAX ||
          !_offlineFile.seek(static_cast<uint32_t>(indexPosition)) ||
          !readFileCooperatively(
              _offlineFile, reinterpret_cast<uint8_t*>(_offlineIndexBlock),
              indexBytes)) {
        continue;
      }
      _offlineActiveBlock = block;
    }

    uint32_t entryLow = 0;
    uint32_t entryHigh = directory.entryCount;
    while (entryLow < entryHigh) {
      const uint32_t middle = entryLow + (entryHigh - entryLow) / 2U;
      const uint64_t candidate = _offlineIndexBlock[middle].key;
      if (candidate < key) entryLow = middle + 1U;
      else entryHigh = middle;
    }
    if (entryLow >= directory.entryCount ||
        _offlineIndexBlock[entryLow].key != key) continue;
    const OfflineIndexEntry& entry = _offlineIndexBlock[entryLow];
    const uint64_t end = static_cast<uint64_t>(entry.offset) + entry.size;
    if (entry.size < 8 || entry.size > MAX_TILE_BYTES ||
        entry.offset < pack.dataOffset || end > _offlineFile.size()) {
      continue;
    }
    destination = static_cast<uint8_t*>(heap_caps_malloc(
        entry.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!destination) return false;
    const bool complete = _offlineFile.seek(entry.offset) &&
        readFileCooperatively(_offlineFile, destination, entry.size);
    const auto validation = complete ? Tab5TileValidation::validate(destination,entry.size) :
        Tab5TileValidation::Result::Unavailable;
    if (validation != Tab5TileValidation::Result::Valid) {
      heap_caps_free(destination); destination = nullptr;
      if (validation == Tab5TileValidation::Result::Invalid) {
        quarantineOffline(packIndex,key);
        Tab5DebugSerial.printf("[MAP] tuile corrompue mise à l’écart dans %s z%u/%lu/%lu\n",
                      pack.name,unsigned(zoom),(unsigned long)x,(unsigned long)y);
      } else return false; // transient I/O/OOM: keep the entry and try later.
      continue;
    }
    size = entry.size;
    return true;
  }
  return false;
}

size_t Tab5MapEngine::offlinePackCountForSelectedStyle() const {
  if (_selectedStyle >= _styleCount) return 0;
  size_t count = 0;
  for (size_t i = 0; i < _offlinePackCount; ++i)
    if (strcmp(_offlinePacks[i].cacheId,
               _styles[_selectedStyle].cacheId) == 0) ++count;
  return count;
}

String Tab5MapEngine::offlinePackSummary() const {
  const size_t count = offlinePackCountForSelectedStyle();
  if (!count) return String();
  if (count == 1) {
    for (size_t i = 0; i < _offlinePackCount; ++i)
      if (strcmp(_offlinePacks[i].cacheId,
                 _styles[_selectedStyle].cacheId) == 0)
        return String(_offlinePacks[i].name);
  }
  return String(count) + " " + Tab5Locale.text(count == 1 ? "paquet SD" : "paquets SD");
}

String Tab5MapEngine::cachePath(uint8_t style, uint8_t zoom, uint32_t x,
                                uint32_t y) const {
  if (style >= _styleCount) return String();
  return String(MAP_CACHE_ROOT) + "/" + _styles[style].cacheId + "/" +
         String(zoom) + "/" + String(x) + "/" + String(y) +
         ".png";
}

bool Tab5MapEngine::cachedTileExists(uint8_t zoom, uint32_t x,
                                     uint32_t y) const {
  if (!_ready || !_sdAvailable || _selectedStyle >= _styleCount) return false;
  return tab5FS.exists(cachePath(_selectedStyle, zoom, x, y).c_str());
}

bool Tab5MapEngine::ensureCacheDirectories(uint8_t style, uint8_t zoom,
                                           uint32_t x) {
  if (!_ready || !_sdAvailable || style >= _styleCount) return false;
  const String providerDir=String(MAP_CACHE_ROOT)+"/"+_styles[style].cacheId;
  const String zoomDir=providerDir+"/"+String(zoom);
  const String xDir=zoomDir+"/"+String(x);
  for(const auto& dir:{String(MAP_ROOT),String(MAP_CACHE_ROOT),providerDir,zoomDir,xDir})
    if (!ensureMapDirectory(dir)) return false;
  return true;
}

bool Tab5MapEngine::copyMemoryTile(uint8_t style, uint8_t zoom, uint32_t x,
                                   uint32_t y, uint8_t*& destination,
                                   size_t& size) {
  if (!_memoryTiles) return false;
  for (size_t i = 0; i < MEMORY_TILE_COUNT; ++i) {
    MemoryTile& tile = _memoryTiles[i];
    if (!tile.data || tile.style >= _styleCount || style >= _styleCount ||
        strcmp(_styles[tile.style].cacheId, _styles[style].cacheId) != 0 ||
        tile.zoom != zoom || tile.x != x || tile.y != y) continue;
    destination = static_cast<uint8_t*>(heap_caps_malloc(
        tile.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!destination) return false;
    memcpy(destination, tile.data, tile.size);
    size = tile.size;
    tile.lastUse = ++_memoryUseCounter;
    if (!_memoryUseCounter) tile.lastUse = ++_memoryUseCounter;
    return true;
  }
  return false;
}

void Tab5MapEngine::forgetMemoryTile(size_t index) {
  if (!_memoryTiles || index >= MEMORY_TILE_COUNT ||
      !_memoryTiles[index].data) return;
  _memoryTileBytes -= _memoryTiles[index].size;
  heap_caps_free(_memoryTiles[index].data);
  _memoryTiles[index] = {};
}

void Tab5MapEngine::rememberTile(uint8_t style, uint8_t zoom, uint32_t x,
                                 uint32_t y, const uint8_t* data,
                                 size_t size) {
  if (!_memoryTiles || !data || size < 8 || size > MEMORY_TILE_LIMIT) return;
  size_t slot = MEMORY_TILE_COUNT;
  for (size_t i = 0; i < MEMORY_TILE_COUNT; ++i) {
    const MemoryTile& tile = _memoryTiles[i];
    if (tile.data && tile.style < _styleCount && style < _styleCount &&
        strcmp(_styles[tile.style].cacheId, _styles[style].cacheId) == 0 &&
        tile.zoom == zoom && tile.x == x && tile.y == y) {
      forgetMemoryTile(i);
      slot = i;
      break;
    }
    if (!tile.data && slot == MEMORY_TILE_COUNT) slot = i;
  }
  while (_memoryTileBytes + size > MEMORY_TILE_LIMIT) {
    size_t oldest = MEMORY_TILE_COUNT;
    for (size_t i = 0; i < MEMORY_TILE_COUNT; ++i) {
      if (!_memoryTiles[i].data) continue;
      if (oldest == MEMORY_TILE_COUNT ||
          _memoryTiles[i].lastUse < _memoryTiles[oldest].lastUse) oldest = i;
    }
    if (oldest == MEMORY_TILE_COUNT) break;
    forgetMemoryTile(oldest);
    if (slot == MEMORY_TILE_COUNT) slot = oldest;
  }
  if (slot == MEMORY_TILE_COUNT) {
    for (size_t i = 0; i < MEMORY_TILE_COUNT; ++i) {
      if (!_memoryTiles[i].data) {
        slot = i;
        break;
      }
    }
  }
  if (slot == MEMORY_TILE_COUNT) {
    size_t oldest = 0;
    for (size_t i = 1; i < MEMORY_TILE_COUNT; ++i)
      if (_memoryTiles[i].lastUse < _memoryTiles[oldest].lastUse) oldest = i;
    forgetMemoryTile(oldest);
    slot = oldest;
  }
  auto* copy = static_cast<uint8_t*>(
      heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!copy) return;
  memcpy(copy, data, size);
  MemoryTile& tile = _memoryTiles[slot];
  tile.data = copy;
  tile.size = size;
  tile.style = style;
  tile.zoom = zoom;
  tile.x = x;
  tile.y = y;
  tile.lastUse = ++_memoryUseCounter;
  if (!_memoryUseCounter) tile.lastUse = ++_memoryUseCounter;
  _memoryTileBytes += size;
}

void Tab5MapEngine::clearMemoryCache() {
  if (!_memoryTiles) return;
  for (size_t i = 0; i < MEMORY_TILE_COUNT; ++i) forgetMemoryTile(i);
  _memoryTileBytes = 0;
}

bool Tab5MapEngine::rejectCachedTile(uint8_t style,uint8_t zoom,uint32_t x,uint32_t y,
                                        const uint8_t* data,size_t size) {
  if (!data || style>=_styleCount || Tab5TileValidation::validate(data,size)!=Tab5TileValidation::Result::Invalid) return false;
  for (size_t i=0;_memoryTiles && i<MEMORY_TILE_COUNT;++i) {
    const auto& tile=_memoryTiles[i];
    if (tile.data && tile.style<_styleCount && !strcmp(_styles[tile.style].cacheId,_styles[style].cacheId) &&
        tile.zoom==zoom && tile.x==x && tile.y==y) forgetMemoryTile(i);
  }
  if (!_sdAvailable) return true;
  const String path=cachePath(style,zoom,x,y); File file=tab5FS.open(path.c_str(),FILE_READ);
  if (!file || file.isDirectory()) {if(file)file.close();return true;}
  const size_t length=file.size();
  // Never remove a different, repaired XYZ tile on the strength of old data.
  if (!length) {file.close(); tab5FS.remove(path.c_str()); return true;}
  if (length>MAX_TILE_BYTES) {file.close();return true;}
  auto* bytes=static_cast<uint8_t*>(heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if (!bytes) {file.close();return true;}
  const bool complete=readFileCooperatively(file,bytes,length);file.close();
  if (complete && Tab5TileValidation::validate(bytes,length)==Tab5TileValidation::Result::Invalid)
    tab5FS.remove(path.c_str());
  heap_caps_free(bytes); return true;
}

bool Tab5MapEngine::readCachedTile(uint8_t zoom,uint32_t x,uint32_t y,uint8_t*& destination,size_t& size) {
  size=0; destination=nullptr;
  if (!_ready || _selectedStyle>=_styleCount) return false;
  if (copyMemoryTile(_selectedStyle,zoom,x,y,destination,size)) {
    const auto validation=Tab5TileValidation::validate(destination,size);
    if(validation==Tab5TileValidation::Result::Valid) return true;
    if(validation==Tab5TileValidation::Result::Invalid) rejectCachedTile(_selectedStyle,zoom,x,y,destination,size);
    heap_caps_free(destination);destination=nullptr;size=0;
    if(validation!=Tab5TileValidation::Result::Invalid) return false;
  }
  if (!_sdAvailable) return false;
  ensureStorageLoaded();
  // A repaired XYZ tile must supersede a corrupt entry in an immutable pack.
  const String path=cachePath(_selectedStyle,zoom,x,y);File file=tab5FS.open(path.c_str(),FILE_READ);
  if (file && !file.isDirectory()) {
    const size_t length=file.size();
    if(length && length<=MAX_TILE_BYTES) {
      destination=static_cast<uint8_t*>(heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
      if(!destination){file.close();return false;}
      const bool complete=readFileCooperatively(file,destination,length);file.close();
      const auto validation=complete ? Tab5TileValidation::validate(destination,length) : Tab5TileValidation::Result::Unavailable;
      if(validation==Tab5TileValidation::Result::Valid) {
        size=length;rememberTile(_selectedStyle,zoom,x,y,destination,size);return true;
      }
      if(validation==Tab5TileValidation::Result::Invalid) tab5FS.remove(path.c_str());
      heap_caps_free(destination);destination=nullptr;
      if(validation!=Tab5TileValidation::Result::Invalid)return false;
    } else {file.close();if(!length)tab5FS.remove(path.c_str());}
  } else if(file) file.close();
  if(readOfflineTile(_selectedStyle,zoom,x,y,destination,size)) {
    rememberTile(_selectedStyle,zoom,x,y,destination,size);return true;
  }
  return false;
}

bool Tab5MapEngine::storeTile(const Tab5MapTileResult& result) {
  if (!_ready || result.style >= _styleCount || !result.success ||
      !validTile(result.data, result.size)) return false;
  rememberTile(result.style, result.zoom, result.x, result.y,
               result.data, result.size);
  if (!ensureCacheDirectories(result.style, result.zoom, result.x)) return false;
  const String path = cachePath(result.style, result.zoom, result.x, result.y);
  const String temporary = path + ".part";
  if (!prepareMapTemporary(temporary)) return false;
  errno=0;
  File file = tab5FS.open(temporary.c_str(), FILE_WRITE);
  if (!file) {tab5StorageFault("ouverture",temporary.c_str());return false;}
  const bool written = writeFileCooperatively(
      file, result.data, result.size);
  if (!closeWrittenMapFile(file, written, result.size)) {
    const int error = errno;
    tab5StorageFault("écriture",temporary.c_str(),error);
    tab5FS.remove(temporary.c_str());
    errno = error;
    return false;
  }
  return publishMapFile(temporary, path);
}

String Tab5MapEngine::styleId(size_t index) const {
  return index < _styleCount ? String(_styles[index].id) : String();
}

String Tab5MapEngine::styleName(size_t index) const {
  return index < _styleCount ? Tab5Locale.text(String(_styles[index].name)) : String();
}

bool Tab5MapEngine::styleIsLightweight(size_t index) const {
  return index < _styleCount && _styles[index].lightweight;
}



uint8_t Tab5MapEngine::styleMinZoom(size_t index) const {
  return index < _styleCount ? _styles[index].minZoom : 0;
}

uint8_t Tab5MapEngine::styleMaxZoom(size_t index) const {
  return index < _styleCount ? _styles[index].maxZoom : 18;
}

int8_t Tab5MapEngine::styleZoomShift(size_t index) const {
  return index < _styleCount ? _styles[index].zoomShift : 0;
}

bool Tab5MapEngine::selectStyle(size_t index) {
  if (index >= _styleCount) return false;
  closeOfflinePack();
  _selectedStyle = static_cast<uint8_t>(index);
  Preferences preferences;
  if (preferences.begin(MAP_PREFS_NAMESPACE, false)) {
    preferences.putString("style", _styles[index].id);
    preferences.end();
  }
  return true;
}

String Tab5MapEngine::attribution() const {
  return _selectedStyle < _styleCount
      ? String(_styles[_selectedStyle].attribution) : String();
}

// Parse names without a temporary String allocation after JSON validation.
// All remaining copies into the live table are then infallible and bounded.
static void copyStoredPlaceName(char* out, size_t capacity, const char* raw) {
  if (!out || !capacity) return;
  while (*raw && static_cast<uint8_t>(*raw) <= 32) ++raw;
  size_t available = strlen(raw);
  while (available && static_cast<uint8_t>(raw[available - 1]) <= 32) --available;
  size_t length = std::min(available, capacity - 1);
  while (length && length < available &&
         (static_cast<uint8_t>(raw[length]) & 0xc0) == 0x80) --length;
  memcpy(out, raw, length);
  for (size_t i = 0; i < length; ++i)
    if (out[i] == '\r' || out[i] == '\n' || out[i] == '\t') out[i] = ' ';
  out[length] = 0;
}

bool Tab5MapEngine::loadPlaces() {
  _placesLoaded = false;
  if (!_ready || !_sdAvailable || !_places) return false;
  const String backup = String(MAP_PLACES_FILE) + ".bak";
  bool primaryPresent = false, backupPresent = false;
  if (!tab5AtomicProbe(MAP_PLACES_FILE, primaryPresent) ||
      (!primaryPresent && !tab5AtomicProbe(backup.c_str(), backupPresent)))
    return false;
  if (!primaryPresent && !backupPresent) {
    _placeCount = 0;
    _placesLoaded = true;
    ++_localMarkerRevision;
    return true;
  }
  // Reading a backup is sufficient after an interrupted publication. Do not
  // rename or replace anything until an explicit subsequent save succeeds.
  const char* path = primaryPresent ? MAP_PLACES_FILE : backup.c_str();
  errno = 0;
  File file = tab5FS.open(path, FILE_READ);
  if (!file || file.isDirectory() || file.size() > MAX_PLACES_BYTES) {
    file.close();
    return false;
  }
  JsonDocument document(tab5JsonAllocator());
  errno = 0;
  const DeserializationError error = deserializeJson(document, file);
  const int readError = errno;
  errno = 0;
  file.close();
  const int closeError = errno;
  if (error || readError || closeError ||
      !document["places"].is<JsonArrayConst>()) return false;

  // I/O, JSON allocation and structure validation have all completed. A
  // failure above preserves the prior in-memory table and forbids saving it
  // as if it were a successfully loaded, possibly empty collection.
  size_t count = 0;
  for (JsonObjectConst item : document["places"].as<JsonArrayConst>()) {
    if (count >= TAB5_MAP_MAX_PLACES) break;
    const double latitude = item["lat"] | 0.0;
    const double longitude = item["lon"] | 0.0;
    if (!Tab5GeoIndex::validCoordinate(latitude, longitude)) continue;
    Tab5MapPlace place{};
    copyStoredPlaceName(place.name, sizeof(place.name), item["name"] | "");
    if (!place.name[0]) continue;
    place.latitude = latitude;
    place.longitude = longitude;
    place.zoom = constrain(static_cast<int>(item["zoom"] | 0), 0, 22);
    _places[count++] = place;
  }
  _placeCount = count;
  _placesLoaded = true;
  ++_localMarkerRevision;
  return true;
}

bool Tab5MapEngine::savePlaces() {
  if (!_ready || !_sdAvailable || !_placesLoaded) return false;
  JsonDocument document(tab5JsonAllocator());
  JsonArray places = document["places"].to<JsonArray>();
  for (size_t i = 0; i < _placeCount; ++i) {
    JsonObject item = places.add<JsonObject>();
    item["name"] = _places[i].name;
    item["lat"] = _places[i].latitude;
    item["lon"] = _places[i].longitude;
    item["zoom"] = _places[i].zoom;
  }
  if (document.overflowed()) return false;
  const String temporary = String(MAP_PLACES_FILE) + ".part";
  if (!prepareMapTemporary(temporary)) return false;
  errno=0;
  File file = tab5FS.open(temporary.c_str(), FILE_WRITE);
  if (!file) {tab5StorageFault("ouverture",temporary.c_str());return false;}
  const size_t expected = measureJsonPretty(document);
  errno = 0;
  const size_t written = serializeJsonPretty(document, file);
  if (!closeWrittenMapFile(file, written == expected, expected)) {
    const int error = errno;
    tab5StorageFault("écriture",temporary.c_str(),error);
    tab5FS.remove(temporary.c_str());
    errno = error;
    return false;
  }
  return publishMapFile(temporary, String(MAP_PLACES_FILE));
}

const Tab5MapPlace* Tab5MapEngine::place(size_t index) const {
  return index < _placeCount ? &_places[index] : nullptr;
}

bool Tab5MapEngine::addPlace(const String& rawName, double latitude,
                             double longitude, uint8_t zoom) {
  if (!_placesLoaded && !loadPlaces()) return false;
  if (_placeCount >= TAB5_MAP_MAX_PLACES||!Tab5GeoIndex::validCoordinate(latitude,longitude)) return false;
  Tab5MapPlace place{};
  copyStoredPlaceName(place.name, sizeof(place.name), rawName.c_str());
  if (!place.name[0]) return false;
  place.latitude = latitude;
  place.longitude = longitude;
  place.zoom = static_cast<uint8_t>(
      constrain(static_cast<int>(zoom), 0, 22));
  _places[_placeCount++] = place;
  if (savePlaces()) {++_localMarkerRevision;return true;}
  --_placeCount;
  return false;
}

bool Tab5MapEngine::renamePlace(size_t index, const String& rawName) {
  if (!_placesLoaded && !loadPlaces()) return false;
  if (index >= _placeCount) return false;
  char name[sizeof(_places[index].name)]{};
  copyStoredPlaceName(name, sizeof(name), rawName.c_str());
  if (!name[0]) return false;
  const Tab5MapPlace previous = _places[index];
  memcpy(_places[index].name, name, sizeof(name));
  if (savePlaces()) {++_localMarkerRevision;return true;}
  _places[index] = previous;
  return false;
}

bool Tab5MapEngine::removePlace(size_t index) {
  if (!_placesLoaded && !loadPlaces()) return false;
  if (index >= _placeCount) return false;
  const Tab5MapPlace removed = _places[index];
  for (size_t i = index; i + 1 < _placeCount; ++i)
    _places[i] = _places[i + 1];
  _places[--_placeCount] = {};
  if (savePlaces()) {++_localMarkerRevision;return true;}
  for (size_t i = _placeCount; i > index; --i)
    _places[i] = _places[i - 1];
  _places[index] = removed;
  ++_placeCount;
  return false;
}

bool Tab5MapEngine::prepareRegionDownload(double latitude, double longitude,
                                           uint8_t zoom) {
  if (!_ready || !_sdAvailable || _selectedStyle >= _styleCount ||
      !isfinite(latitude) || !isfinite(longitude) ||
      latitude < -85.0511287798066 || latitude > 85.0511287798066 ||
      longitude < -180.0 || longitude > 180.0 || zoom > 22) return false;
  Tab5StorageUse storage{true};
  if (!storage) return false;
  JsonDocument document(tab5JsonAllocator());
  document["version"] = 1;
  document["lat"] = latitude;
  document["lon"] = longitude;
  document["zoom"] = zoom;
  document["source_zoom"] = styleSourceZoom(_selectedStyle, zoom);
  const MapStyle& style = _styles[_selectedStyle];
  document["provider"] = !strcmp(style.cacheId, "opentopomap") ? "opentopomap" : "osm";
  document["url"] = style.url;
  if (document.overflowed()) return false;
  if (!ensureMapDirectory(String(MAP_ROOT))) return false;
  const String temporary = String(MAP_REGION_REQUEST) + ".part";
  if (!prepareMapTemporary(temporary)) return false;
  errno = 0;
  File file = tab5FS.open(temporary.c_str(), FILE_WRITE);
  if (!file) { tab5StorageFault("ouverture", temporary.c_str()); return false; }
  const size_t expected = measureJson(document);
  errno = 0;
  const size_t written = serializeJson(document, file);
  if (!closeWrittenMapFile(file, written == expected, expected)) {
    const int error = errno;
    tab5StorageFault("écriture", temporary.c_str(), error);
    tab5FS.remove(temporary.c_str());
    errno = error;
    return false;
  }
  return publishMapFile(temporary, String(MAP_REGION_REQUEST));
}

bool Tab5MapEngine::setRememberView(bool enabled) {
  if (enabled == _rememberView) return true;
  Preferences preferences;
  if (!preferences.begin(MAP_PREFS_NAMESPACE, false)) return false;
  const bool ok = preferences.putBool("remember", enabled) == sizeof(bool);
  preferences.end();
  if (ok) _rememberView = enabled;
  return ok;
}

static bool mapLoadStoredView(const char* key, const char* latitudeKey,
                             const char* longitudeKey, const char* zoomKey,
                             double& latitude, double& longitude, uint8_t& zoom) {
  Preferences preferences;
  if (!preferences.begin(MAP_PREFS_NAMESPACE, true)) return false;
  double lat = latitude, lon = longitude;
  uint8_t z = zoom;
  bool ok = false;
  if (preferences.isKey(key)) {
    Tab5MapViewState::Record record;
    ok = preferences.getBytesLength(key) == sizeof(record) &&
        preferences.getBytes(key, &record, sizeof(record)) == sizeof(record) &&
        Tab5MapViewState::decode(record, lat, lon, z);
  } else if (preferences.isKey(latitudeKey) && preferences.isKey(longitudeKey) &&
             preferences.isKey(zoomKey)) {
    // Read-only migration of user settings from <= 0.8.23. A damaged new
    // record never falls back to an older, potentially unrelated location.
    lat = preferences.getDouble(latitudeKey, NAN);
    lon = preferences.getDouble(longitudeKey, NAN);
    z = preferences.getUChar(zoomKey, UINT8_MAX);
    ok = Tab5MapViewState::valid(lat, lon, z);
  }
  preferences.end();
  if (ok) { latitude = lat; longitude = lon; zoom = z; }
  return ok;
}

static bool mapSaveStoredView(const char* key, double latitude,
                             double longitude, uint8_t zoom) {
  Tab5MapViewState::Record record, previous;
  if (!Tab5MapViewState::encode(latitude, longitude, zoom, record)) return false;
  Preferences preferences;
  if (!preferences.begin(MAP_PREFS_NAMESPACE, false)) return false;
  const bool unchanged = preferences.getBytesLength(key) == sizeof(previous) &&
      preferences.getBytes(key, &previous, sizeof(previous)) == sizeof(previous) &&
      std::memcmp(&record, &previous, sizeof(record)) == 0;
  const bool ok = unchanged ||
      preferences.putBytes(key, &record, sizeof(record)) == sizeof(record);
  preferences.end();
  return ok;
}

bool Tab5MapEngine::loadView(double& latitude, double& longitude, uint8_t& zoom) {
  return _rememberView &&
      mapLoadStoredView("view", "lat", "lon", "zoom", latitude, longitude, zoom);
}

bool Tab5MapEngine::saveView(double latitude, double longitude, uint8_t zoom) {
  return !_rememberView || mapSaveStoredView("view", latitude, longitude, zoom);
}

bool Tab5MapEngine::loadHomeView(double& latitude, double& longitude,
                                 uint8_t& zoom) {
  return mapLoadStoredView("homeView", "home_lat", "home_lon", "home_zoom",
                           latitude, longitude, zoom);
}

bool Tab5MapEngine::saveHomeView(double latitude, double longitude,
                                 uint8_t zoom) {
  return mapSaveStoredView("homeView", latitude, longitude, zoom);
}

namespace {
class GazetteerReader {
 public:
  explicit GazetteerReader(File& source):file(source),bytes(source.size()) {
    buffer=static_cast<uint8_t*>(heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  }
  ~GazetteerReader(){heap_caps_free(buffer);}
  bool valid()const{return buffer!=nullptr;}
  uint32_t size()const{return bytes;}
  bool read(uint32_t offset,void* destination,size_t count){
    if(!buffer||offset>bytes||count>bytes-offset)return false;
    auto* out=static_cast<uint8_t*>(destination);
    while(count){
      const uint32_t block=offset&~uint32_t(4095);
      if(block!=cached){
        cached=UINT32_MAX;validBytes=0;
        if(!file.seek(block))return false;
        const size_t need=std::min<size_t>(4096,bytes-block);
        while(validBytes<need){const size_t got=file.read(buffer+validBytes,need-validBytes);if(!got||got>need-validBytes)return false;validBytes+=got;}
        cached=block;
      }
      const size_t local=offset-block;if(local>=validBytes)return false;
      const size_t part=std::min(count,validBytes-local);memcpy(out,buffer+local,part);out+=part;offset+=part;count-=part;
    }
    return true;
  }
 private:
  File& file;uint32_t bytes=0,cached=UINT32_MAX;size_t validBytes=0;uint8_t* buffer=nullptr;
};
}

bool Tab5MapEngine::localSearch(const SearchRequest& request,Tab5MapSearchResult& result) {
  result.local=true;
  result.items=static_cast<Tab5MapSearchItem*>(heap_caps_calloc(TAB5_MAP_MAX_SEARCH_RESULTS,sizeof(Tab5MapSearchItem),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!result.items){result.httpCode=-103;return false;}
  char query[Tab5GeoIndex::MaxKey+1];
  if(!Tab5GeoIndex::normalize(request.query,query,sizeof(query))){result.httpCode=-106;return false;}
  double distances[TAB5_MAP_MAX_SEARCH_RESULTS]{};bool exacts[TAB5_MAP_MAX_SEARCH_RESULTS]{};
  uint8_t priorities[TAB5_MAP_MAX_SEARCH_RESULTS]{};uint32_t populations[TAB5_MAP_MAX_SEARCH_RESULTS]{};
  const auto add=[&](const Tab5MapSearchItem& item,bool exact,uint8_t priority,uint32_t population){
    const double distance=Tab5GeoIndex::distanceSquared(request.latitude,request.longitude,item.latitude,item.longitude);
    for(unsigned i=0;i<result.count;++i){
      const auto& previous=result.items[i];
      if(previous.node!=item.node||fabs(previous.latitude-item.latitude)>=0.00001||fabs(previous.longitude-item.longitude)>=0.00001||strcmp(previous.name,item.name))continue;
      if(priority>=priorities[i])return;
      // A selected search pin must not hide the corresponding live Mesh node.
      for(unsigned j=i+1;j<result.count;++j){result.items[j-1]=result.items[j];distances[j-1]=distances[j];exacts[j-1]=exacts[j];priorities[j-1]=priorities[j];populations[j-1]=populations[j];}
      --result.count;break;
    }
    unsigned at=0;
    while(at<result.count){
      const double band=std::floor(std::sqrt(distance)*2),previousBand=std::floor(std::sqrt(distances[at])*2);
      bool better=exact!=exacts[at]?exact:priority!=priorities[at]?priority<priorities[at]:band!=previousBand?band<previousBand:population!=populations[at]?population>populations[at]:distance<distances[at];
      if(better)break;++at;
    }
    if(at>=TAB5_MAP_MAX_SEARCH_RESULTS)return;
    const unsigned last=std::min<unsigned>(result.count,TAB5_MAP_MAX_SEARCH_RESULTS-1);
    for(unsigned i=last;i>at;--i){result.items[i]=result.items[i-1];distances[i]=distances[i-1];exacts[i]=exacts[i-1];priorities[i]=priorities[i-1];populations[i]=populations[i-1];}
    result.items[at]=item;distances[at]=distance;exacts[at]=exact;priorities[at]=priority;populations[at]=population;
    if(result.count<TAB5_MAP_MAX_SEARCH_RESULTS)++result.count;
  };
  for(unsigned i=0;i<request.pointCount;++i){
    const auto& p=request.points[i];char name[Tab5GeoIndex::MaxKey+1];
    if(!Tab5GeoIndex::validCoordinate(p.latitude,p.longitude)||!Tab5GeoIndex::normalize(p.name,name,sizeof(name)))continue;
    const char* matched=strstr(name,query);char id[16];snprintf(id,sizeof(id),"%08lx",static_cast<unsigned long>(p.node));
    if(!matched&&(!p.node||!strstr(id,query)))continue;
    Tab5MapSearchItem item{};Tab5GeoIndex::copy(item.name,sizeof(item.name),p.name);item.latitude=p.latitude;item.longitude=p.longitude;item.kind=p.kind;item.node=p.node;item.timestamp=p.timestamp;item.zoom=15;
    if(p.kind==Tab5MapMarkerKind::Mesh)snprintf(item.displayName,sizeof(item.displayName),"Meshtastic · !%08lx",static_cast<unsigned long>(p.node));
    add(item,!strcmp(name,query),p.kind==Tab5MapMarkerKind::Search?2:0,0);
  }
  const auto cancelled=[&](){vTaskDelay(1);return !_active||request.requestId!=_searchRequestId;};
  if(cancelled()){result.httpCode=-107;return false;}
  {
    Tab5StorageUse storage{true};
    if(storage&&tab5SdAvailable()){
      File file=tab5FS.open("/System/maps/gazetteer/cities.t5geo",FILE_READ);
      if(file&&!file.isDirectory()&&file.size()<=128u*1024u*1024u){
        GazetteerReader reader(file);
        if(reader.valid()){
          Tab5GeoIndex::Result found[Tab5GeoIndex::MaxResults];
          const auto outcome=Tab5GeoIndex::search(reader,request.query,request.latitude,request.longitude,found,Tab5GeoIndex::MaxResults,cancelled);
          result.gazetteerAvailable=outcome.status==Tab5GeoIndex::Status::Ok;
          result.truncated=outcome.truncated;
          if(outcome.status==Tab5GeoIndex::Status::Cancelled){file.close();result.httpCode=-107;return false;}
          if(result.gazetteerAvailable){
            for(unsigned i=0;i<outcome.count;++i){const auto& g=found[i];Tab5MapSearchItem item{};
              Tab5GeoIndex::copy(item.name,sizeof(item.name),g.name);item.latitude=g.latitude;item.longitude=g.longitude;
              item.zoom=g.population>=1000000?10:g.population>=100000?12:14;
              snprintf(item.displayName,sizeof(item.displayName),"%s · %s%s%s",g.name,g.country,g.area[0]?" · ":"",g.area);
              add(item,g.exact,1,g.population);
            }
          }else result.localError=true;
        }else result.localError=true;
        file.close();
      }else{if(file)file.close();result.localError=true;}
    }else result.localError=true;
  }
  if(cancelled()){result.httpCode=-107;return false;}
  result.success=true;return true;
}

size_t Tab5MapEngine::markerCount() const {
  return _placeCount+(_haveSearchMarker&&(!_searchMarker.node||Tab5Meshtastic::options().showOnMap)?1:0)+(Tab5Meshtastic::options().showOnMap?std::min<size_t>(32,Tab5Meshtastic::positionCount()):0);
}
bool Tab5MapEngine::marker(size_t index,Tab5MapMarker& out) const {
  out={};
  if(index<_placeCount){const auto& p=_places[index];Tab5GeoIndex::copy(out.name,sizeof(out.name),p.name);out.latitude=p.latitude;out.longitude=p.longitude;out.kind=Tab5MapMarkerKind::Saved;return Tab5GeoIndex::validCoordinate(out.latitude,out.longitude);}
  index-=_placeCount;
  if(_haveSearchMarker&&(!_searchMarker.node||Tab5Meshtastic::options().showOnMap)){if(!index){out=_searchMarker;return true;}--index;}
  if(!Tab5Meshtastic::options().showOnMap||index>=32)return false;
  Tab5Meshtastic::Position position;if(!Tab5Meshtastic::position(index,position)||!Tab5GeoIndex::validCoordinate(position.latitude,position.longitude))return false;
  Tab5GeoIndex::copy(out.name,sizeof(out.name),position.name);out.latitude=position.latitude;out.longitude=position.longitude;out.node=position.node;out.timestamp=position.timestamp;out.kind=Tab5MapMarkerKind::Mesh;return true;
}
uint32_t Tab5MapEngine::markerRevision() const {
  const bool shown=Tab5Meshtastic::options().showOnMap;const uint32_t revision=shown?Tab5Meshtastic::positionRevision():0;
  if(_seenLocalMarkerRevision!=_localMarkerRevision||shown!=_seenMeshVisible||revision!=_seenMeshRevision){_seenLocalMarkerRevision=_localMarkerRevision;_seenMeshVisible=shown;_seenMeshRevision=revision;++_markerRevision;}
  return _markerRevision;
}
void Tab5MapEngine::selectSearchMarker(const Tab5MapSearchItem& item) {
  if(!Tab5GeoIndex::validCoordinate(item.latitude,item.longitude))return;
  _searchMarker={};Tab5GeoIndex::copy(_searchMarker.name,sizeof(_searchMarker.name),item.name);
  _searchMarker.latitude=item.latitude;_searchMarker.longitude=item.longitude;_searchMarker.node=item.node;_searchMarker.timestamp=item.timestamp;_searchMarker.kind=item.kind;
  _haveSearchMarker=true;++_localMarkerRevision;
}
