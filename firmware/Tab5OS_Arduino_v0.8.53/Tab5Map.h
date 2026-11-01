#pragma once

#include <Arduino.h>
#include "Tab5MapViewPolicy.h"
#include "Tab5MapViewState.h"
#include <atomic>
#include <freertos/semphr.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

static constexpr size_t TAB5_MAP_MAX_STYLES = 9;
static constexpr size_t TAB5_MAP_MAX_PLACES = 64;
static constexpr size_t TAB5_MAP_MAX_SEARCH_RESULTS = 8;
static constexpr size_t TAB5_MAP_MAX_OFFLINE_PACKS = 128;

struct Tab5MapTileResult {
  uint8_t zoom = 0;
  uint8_t style = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t generation = 0;
  uint8_t* data = nullptr;
  size_t size = 0;
  int httpCode = 0;
  int tlsCode = 0;
  uint8_t attempts = 0;
  bool success = false;
};

struct Tab5MapPlace {
  char name[64] = {};
  double latitude = 0.0;
  double longitude = 0.0;
  uint8_t zoom = 0;
};

enum class Tab5MapMarkerKind : uint8_t { Saved, Mesh, Search };
struct Tab5MapMarker {
  char name[96] = {};
  double latitude=0,longitude=0;
  uint32_t node=0,timestamp=0;
  Tab5MapMarkerKind kind=Tab5MapMarkerKind::Saved;
};

struct Tab5MapSearchItem {
  char name[96] = {};
  char alternative[96] = {};
  char displayName[192] = {};
  double latitude = 0.0;
  double longitude = 0.0;
  uint8_t zoom = 14;
  uint32_t node=0,timestamp=0;
  Tab5MapMarkerKind kind=Tab5MapMarkerKind::Search;
};

struct Tab5MapSearchResult {
  bool local=false,gazetteerAvailable=false,truncated=false,localError=false;
  uint32_t requestId = 0;
  Tab5MapSearchItem* items = nullptr;
  uint8_t count = 0;
  int httpCode = 0;
  int tlsCode = 0;
  uint8_t attempts = 0;
  bool success = false;
};

// Downloads only current viewport tiles. Tile cache writes remain UI-owned.
// The same bounded worker reads the local gazetteer under a storage lease;
// it never retains its SD handle while doing network/TLS work.
struct Tab5MapConnection;
class Tab5MapEngine {
 public:
  bool begin(bool sdAvailable, BaseType_t taskCore);
  // UI only, under the exclusive SD recovery guard. Worker reads hold a storage lease.
  void setStorageAvailable(bool available);
  void setActive(bool active, uint32_t generation);
  void beginGeneration(uint32_t generation);

  bool requestTile(uint8_t zoom, uint32_t x, uint32_t y,
                   uint32_t generation, bool regionImport = false);
  bool styleImportAllowed(size_t index) const;
  uint16_t importRateTenths() const { return _importRateTenths.load(); }
  uint16_t importEffectiveRateTenths(size_t index, uint16_t preview=0) const;
  bool setImportRateTenths(uint16_t value);
  bool poll(Tab5MapTileResult& result);
  void release(Tab5MapTileResult& result);

  // Prefer the available Internet connection, then fall back locally on an
  // unavailable/failed/empty lookup. This preference never turns Wi-Fi on.
  bool requestSearch(const String& query, uint32_t requestId,
                     double latitude=0,double longitude=0,bool online=true);
  // UI-owned markers; Mesh snapshots are copied through its short lock.
  size_t markerCount() const;
  bool marker(size_t index,Tab5MapMarker& out) const;
  uint32_t markerRevision() const;
  void selectSearchMarker(const Tab5MapSearchItem& item);
  bool pollSearch(Tab5MapSearchResult& result);
  void releaseSearch(Tab5MapSearchResult& result);

  // Caller owns the returned exact-sized PSRAM allocation: heap_caps_free().
  bool readCachedTile(uint8_t zoom, uint32_t x, uint32_t y,
                      uint8_t*& destination, size_t& size);
  bool storeTile(const Tab5MapTileResult& result);
  // Returns true only when a fully read source is decoder-proven corrupt.
  // A temporary allocation/decoder/storage failure never evicts user data.
  bool rejectCachedTile(uint8_t style, uint8_t zoom, uint32_t x, uint32_t y,
                        const uint8_t* data, size_t size);
  bool cachedTileExists(uint8_t zoom, uint32_t x, uint32_t y) const;
  void clearMemoryCache();
  // UI only, under the exclusive SD recovery guard. Reopens lazily.
  void releaseStorageHandles() { closeOfflinePack(); }
  size_t memoryCacheBytes() const { return _memoryTileBytes; }
  size_t offlinePackCount() const {
    return _offlinePackCount;
  }
  size_t offlinePackCountForSelectedStyle() const;
  String offlinePackSummary() const;

  size_t styleCount() const { return _styleCount; }
  size_t selectedStyle() const { return _selectedStyle; }
  String styleId(size_t index) const;
  String styleName(size_t index) const;
  bool styleIsLightweight(size_t index) const;
  uint8_t styleMinZoom(size_t index) const;
  uint8_t styleMaxZoom(size_t index) const;
  int8_t styleZoomShift(size_t index) const;
  uint8_t styleViewMaxZoom(size_t index) const { return Tab5MapViewPolicy::displayMax(styleMaxZoom(index), styleZoomShift(index)); }
  uint8_t styleSourceZoom(size_t index, uint8_t view) const { return Tab5MapViewPolicy::sourceZoom(view, styleMaxZoom(index), styleZoomShift(index)); }
  bool selectStyle(size_t index);

  size_t placeCount() const { return _placeCount; }
  const Tab5MapPlace* place(size_t index) const;
  bool addPlace(const String& name, double latitude, double longitude,
                uint8_t zoom);
  bool renamePlace(size_t index, const String& name);
  bool removePlace(size_t index);

  // Writes one bounded launch request for the tablet region downloader.
  bool prepareRegionDownload(double latitude, double longitude, uint8_t zoom);

  // UI thread only. Save on leaving Cartes; this never touches the SD card.
  bool rememberView() const { return _rememberView; }
  bool setRememberView(bool enabled);
  bool loadView(double& latitude, double& longitude, uint8_t& zoom);
  bool saveView(double latitude, double longitude, uint8_t zoom);
  bool loadHomeView(double& latitude, double& longitude, uint8_t& zoom);
  bool saveHomeView(double latitude, double longitude, uint8_t zoom);

  bool ready() const { return _ready; }
  bool storageAvailable() const { return _sdAvailable; }
  size_t queuedRequests() const;
  String attribution() const;

 private:
  struct MapStyle {
    char id[24];
    char cacheId[24];
    char name[48];
    char url[512];
    char attribution[128];
    uint8_t minZoom;
    uint8_t maxZoom;
    int8_t zoomShift;
    bool lightweight;
};

  struct TileRequest {
    uint8_t zoom;
    uint8_t style;
    uint32_t x;
    uint32_t y;
    uint32_t generation;
    bool regionImport;
  };

  struct SearchRequest {
    char query[385]; // 96 UI characters, including four-byte UTF-8.
    // Snapshot the UI language on the UI thread; the network task must not
    // read the dictionary while the user changes their SD language pack.
    char language[24];
    uint32_t requestId;
    double latitude,longitude;
    bool online;
    // UI snapshot on PSRAM. Ownership moves to the queue/worker on success.
    Tab5MapMarker* points;
    uint8_t pointCount;
  };

  struct MemoryTile {
    uint8_t* data;
    size_t size;
    uint32_t x;
    uint32_t y;
    uint32_t lastUse;
    uint8_t zoom;
    uint8_t style;
  };

  struct OfflinePack {
    char path[128];
    char cacheId[24];
    char name[40];
    uint32_t entryCount;
    uint32_t directoryCount;
    uint32_t entriesPerBlock;
    uint32_t directoryOffset;
    uint32_t indexOffset;
    uint32_t dataOffset;
    uint64_t minKey;
    uint64_t maxKey;
  };

  struct OfflineDirectoryEntry {
    uint64_t firstKey;
    uint32_t firstEntry;
    uint32_t entryCount;
  };

  struct OfflineIndexEntry {
    uint64_t key;
    uint32_t offset;
    uint32_t size;
  };

  static void taskEntry(void* parameter);
  void taskLoop();
  bool createQueues();
  bool downloadTile(const TileRequest& request, Tab5MapTileResult& result, Tab5MapConnection& connection);
  bool downloadSearch(const SearchRequest& request,
                      Tab5MapSearchResult& result, Tab5MapConnection& connection);
  bool localSearch(const SearchRequest& request,Tab5MapSearchResult& result);
  void discardSearchRequests();
  bool ensureTask();

  void addDefaultStyles();
  bool loadStylesConfig();
  bool loadPlaces();
  bool savePlaces();
  bool ensureCacheDirectories(uint8_t style, uint8_t zoom, uint32_t x);
  String cachePath(uint8_t style, uint8_t zoom, uint32_t x,
                   uint32_t y) const;
  String expandedUrl(uint8_t style, uint8_t zoom, uint32_t x,
                     uint32_t y) const;
  bool copyMemoryTile(uint8_t style, uint8_t zoom, uint32_t x, uint32_t y,
                      uint8_t*& destination, size_t& size);
  void rememberTile(uint8_t style, uint8_t zoom, uint32_t x, uint32_t y,
                    const uint8_t* data, size_t size);
  void forgetMemoryTile(size_t index);
  bool loadOfflinePacks();
  bool readOfflineHeader(const char* path, OfflinePack& pack);
  bool activateOfflinePack(size_t index);
  void closeOfflinePack();
  bool readOfflineTile(uint8_t style, uint8_t zoom, uint32_t x, uint32_t y,
                       uint8_t*& destination, size_t& size);
  bool quarantinedOffline(size_t pack, uint64_t key) const;
  void quarantineOffline(size_t pack, uint64_t key);
  static uint64_t offlineTileKey(uint8_t zoom, uint32_t x, uint32_t y);
  bool allocateRuntimeStorage();
  void ensureStorageLoaded();

  bool _ready = false;
  bool _rememberView = true;
  bool _sdAvailable = false;
  bool _storageLoaded = false;
  bool _placesLoaded = false;
  bool _stylesConfigLoaded = false;
  std::atomic<bool> _active{false};
  std::atomic<uint32_t> _generation{0};
  std::atomic<uint32_t> _searchRequestId{0};
  BaseType_t _taskCore = tskNO_AFFINITY;
  StaticQueue_t _queueControls[4]{};
  uint8_t* _queueStorage = nullptr;
  QueueHandle_t _requestQueue = nullptr;
  QueueHandle_t _resultQueue = nullptr;
  QueueHandle_t _searchRequestQueue = nullptr;
  QueueHandle_t _searchResultQueue = nullptr;
  TaskHandle_t _task = nullptr;
  StaticSemaphore_t _lifecycleStorage;
  SemaphoreHandle_t _lifecycleMutex = nullptr;

  // Les tables Cartes vivent en PSRAM afin de réserver la RAM interne aux
  // allocations TLS, ESP-AES et ESP-Hosted.
  MapStyle* _styles = nullptr;
  size_t _styleCount = 0;
  uint8_t _selectedStyle = 0;
  char _userAgent[96] = {};
  char _searchUrl[160] = {};

  Tab5MapPlace* _places = nullptr;
  size_t _placeCount = 0;
  Tab5MapMarker _searchMarker{};
  bool _haveSearchMarker=false;
  uint32_t _localMarkerRevision=1;
  mutable uint32_t _markerRevision=1,_seenLocalMarkerRevision=0,_seenMeshRevision=0;
  mutable bool _seenMeshVisible=false;

  static constexpr size_t MEMORY_TILE_COUNT = 24;
  static constexpr size_t MEMORY_TILE_LIMIT = 2U * 1024U * 1024U;
  MemoryTile* _memoryTiles = nullptr;
  size_t _memoryTileBytes = 0;
  uint32_t _memoryUseCounter = 0;

  struct QuarantinedTile { uint64_t key; uint16_t pack; };
  // Bounded PSRAM table: never disable an entire immutable offline archive.
  QuarantinedTile* _quarantinedTiles = nullptr;
  unsigned _quarantineCount = 0, _quarantineNext = 0;
  OfflinePack* _offlinePacks = nullptr;
  size_t _offlinePackCount = 0;
  fs::File _offlineFile;
  int8_t _offlineActivePack = -1;
  OfflineDirectoryEntry* _offlineDirectory = nullptr;
  OfflineIndexEntry* _offlineIndexBlock = nullptr;
  uint32_t _offlineIndexCapacity = 0;
  uint32_t _offlineActiveBlock = UINT32_MAX;
  std::atomic<uint16_t> _importRateTenths{5};
  uint32_t _lastImportRequestMs=0; // network worker only, retained across reopen
  bool _haveImportRequest=false;
  };

extern Tab5MapEngine Tab5Map;
