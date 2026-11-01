#pragma once
#include "Tab5CatalogueRevision.h"

#include <Arduino.h>
#include <FS.h>
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <vector>
#include <atomic>

#include "Tab5AudioDriver.h"
#include "Tab5AudioPriority.h"
#include "Tab5AudioWorkerLifecycle.h"

class Tab5BufferedRadioSource;
class Tab5NetworkLease;
class AudioFileSource;
class AudioFileSourceSD;
class AudioGeneratorMP3;
class AudioGeneratorAAC;
class AudioGeneratorWAV;
class AudioGeneratorFLAC;

enum class Tab5AudioMode : uint8_t {
  Idle,
  Local,
  Radio,
  Recording,
};

struct Tab5AudioStation {
  String name;
  String style;
  String url;
};

struct Tab5WifiNetwork {
  String ssid;
  int32_t rssi = 0;
  bool secured = true;
};

// v0.3.13 : l'interface ne modifie plus le moteur directement. Chaque action
// devient une commande déposée dans une file ; la tâche audio l'exécute. Les
// opérations longues (poignée de main TLS, préchargement, fermeture du flux)
// ne se déroulent donc plus sur le thread LVGL, qui ne peut plus se figer.
enum class Tab5AudioCommandType : uint8_t {
  None,
  PlayTrack,
  PlayPath,
  SeekPermille,
  PlayStation,
  Next,
  Previous,
  TogglePause,
  Stop,
  StartRecording,
  StopRecording,
  WifiScan,
  WifiConnect,
  WifiEnable,
  WifiForget,
};

struct Tab5AudioCommand {
  Tab5AudioCommandType type = Tab5AudioCommandType::None;
  uint32_t index = 0;
  uint32_t catalogueRevision = 0;
  bool flag = false;
  char text[192] = {};
  char secret[72] = {};
};

struct Tab5AudioRoutingSnapshot {
  Tab5AudioMode mode;
  bool paused;
  bool playing;
  bool wifiConnected;
  uint32_t radioBufferedBytes;
  uint32_t radioBufferCapacity;
};

struct Tab5AudioSnapshot {
  Tab5AudioMode mode = Tab5AudioMode::Idle;
  bool paused = false;
  bool playing = false;
  bool recording = false;
  bool wifiEnabled = false;
  bool wifiConnected = false;
  bool wifiConnecting = false;
  bool wifiAutoStart = false;
  bool hasSavedWifi = false;
  int wifiRssi = 0;
  uint8_t volume = 150;
  uint16_t recordingLevel = 0;
  uint16_t playbackProgressPermille = 0;
  uint32_t recordingSeconds = 0;
  uint32_t playbackElapsedSeconds = 0;
  uint32_t playbackEstimatedSeconds = 0;
  uint32_t radioBufferedBytes = 0;
  uint32_t radioBufferCapacity = 0;
  String nowPlaying;
  String status;
  String lastRecordingPath;
  String currentPath;
  String currentStationUrl;
  String savedWifiSsid;
};

class Tab5AudioEngine {
 public:
  bool begin(bool sdAvailable, uint8_t initialVolume);
  void service();
  // UI only, while holding Tab5StorageRecovery. The worker cannot change handles.
  bool storageIdle() const;
  // Call only while holding Tab5StorageRecovery and storageIdle() is true.
  void setStorageAvailable(bool available);

  void reload();
  // Index media on first opening, rather than delaying the desktop at boot.
  void ensureLibraryLoaded();
  void reloadLibrary();
  // UI memory reclaimer: keeps active playback, recordings and queued actions.
  size_t releaseIdleCatalogues();
  // UI memory callback: the owning worker retires itself only when quiescent.
  size_t reclaimIdleWorker();
  // Worker only: a print may borrow playback memory after ordinary cache
  // reclamation fails. The audio worker closes its own resources; recordings
  // are never interrupted. A generation prevents stale releases.
  uint32_t acquireMemoryPriority(uint32_t timeoutMs = 7000);
  void releaseMemoryPriority(uint32_t token);
  uint32_t trackCatalogueRevision() const { return tab5CatalogueRevision(_trackCatalogueRevision); }
  uint32_t stationCatalogueRevision() const { return tab5CatalogueRevision(_stationCatalogueRevision); }
  size_t trackCount() const;
  String trackPath(size_t index) const;
  String trackName(size_t index) const;
  size_t stationCount() const;
  bool station(size_t index, Tab5AudioStation& result) const;

  bool playTrack(size_t index, uint32_t revision = 0);
  bool playPath(const String& path);
  bool seekPermille(uint16_t permille);
  bool playStation(size_t index, uint32_t revision = 0);
  void previous();
  void next();
  void togglePause();
  void stop();

  bool startRecording();
  bool stopRecording();

  bool startWifiScan();
  bool connectWifi(const String& ssid, const String& password,
                   bool remember = true);
  bool setWifiEnabled(bool enabled);
  void setWifiAutoStart(bool enabled);
  void requestWifiSignal() { _wifiSignalRequested.store(true, std::memory_order_release); }
  void setWifiPowerSave(bool enabled) { _wifiPowerSave = enabled; }
  void forgetWifi();
  size_t wifiNetworkCount() const;
  bool wifiNetwork(size_t index, Tab5WifiNetwork& result) const;
  bool wifiScanRunning() const;
  uint32_t wifiScanRevision() const;
  String wifiScanStatus() const;
  bool hasSavedWifi() const;
  String savedWifiSsid() const;
  bool wifiEnabled() const;
  bool wifiAutoStart() const;
  bool wifiConnecting() const;

  void setVolume(uint8_t value);
  uint8_t volume() const;
  Tab5AudioMode mode() const;
  bool paused() const;
  bool recording() const;
  bool playing() const;
  bool wifiConnected() const;
  int wifiRssi() const;
  uint32_t recordingSeconds() const;
  uint16_t playbackProgressPermille() const;
  uint32_t playbackElapsedSeconds() const;
  uint32_t playbackEstimatedSeconds() const;
  uint16_t recordingLevel() const;
  String nowPlaying() const;
  String status() const;
  String lastRecordingPath() const;
  String currentPath() const;
  bool snapshot(Tab5AudioSnapshot& result) const;
  // Independent UI observers; unchanged indicators do not copy strings or lock.
  // Includes mode, pause, volume, title and playback identities, not progress.
  bool indicatorSnapshot(Tab5AudioSnapshot& result, uint32_t& seen,
                         bool force = false) const;
  // Media controls also observe progress, elapsed time, status and recording
  // level. Each observer keeps its own revision; a failed copy is retried.
  bool mediaSnapshot(Tab5AudioSnapshot& result, uint32_t& seen,
                     bool force = false) const;
  bool wifiSnapshot(bool& enabled, bool& connected) const;
  // Scalar state for frequent routing checks; never copies playback strings.
  bool routingSnapshot(Tab5AudioRoutingSnapshot& result) const;
  bool takeUiDirty();

 private:
  class StateGuard;
  static void audioTaskEntry(void* argument);
  bool startAudioTask(bool essential);
  bool canRetireAudioTask() const;
  bool lockState(TickType_t timeout = portMAX_DELAY) const;
  void unlockState() const;
  void publishSnapshot();
  void serviceWifiSignal();
  void ensureAudioFiles();
  void scanFolder(const String& folder, uint8_t depth);
  void loadStations();
  void reloadStationsPreservingPlayback();
  void addFallbackStations();
  bool loadWifiCredentials();
  bool saveWifiCredentials();
  void serviceWifiScan();
  void setWifiScanStatus(const char* message, bool clearNetworks = false);
  void serviceWifiConnection();
  void serviceWifiPower();
  bool wifiPowerTarget(uint32_t now);
  bool startRadioNow(size_t index, uint32_t revision = 0);
  bool startLocalDecoder(const String& path);
  bool selectAdjacentTrack(int direction, bool recordingsOnly);
  bool selectAdjacentStation(int direction);

  // File de commandes : postCommand() est appelé par l'interface,
  // drainCommands()/handleCommand() par la tâche audio uniquement.
  bool postCommand(const Tab5AudioCommand& command);
  void drainCommands();
  void handleCommand(const Tab5AudioCommand& command);
  void serviceDecoder();
  void serviceMemoryPriority();
  void serviceIdleMemoryPriority();
  bool deferPriorityCommand(const Tab5AudioCommand& command);
  void serviceTelemetry();

  // Variantes exécutées sur la tâche audio. Elles ne tiennent le mutex d'état
  // que pour lire ou écrire les listes partagées, jamais pendant le réseau.
  bool execPlayTrack(size_t index, uint32_t revision = 0);
  bool execPlayPath(const String& path);
  bool execSeekPermille(uint16_t permille);
  bool execPlayStation(size_t index, uint32_t revision = 0);
  bool resumeSelectedStation();
  void execPrevious();
  void execNext();
  void execTogglePause();
  void execStop();
  bool execStartRecording();
  bool execStopRecording();
  bool execStartWifiScan();
  bool execConnectWifi(const String& ssid, const String& password,
                       bool remember);
  bool execSetWifiEnabled(bool enabled);
  void execForgetWifi();
  void stopPlayback(bool clearIntent, bool preserveTransport = false);
  void cleanupDecoder(bool preserveTransport = false);
  bool ensureMp3CodecBuffer();
  bool ensureAacCodecBuffer();
  bool decoderRunning() const;
  bool decoderLoop();
  void setStatus(const String& value);
  void setNowPlaying(const String& value);
  String makeRecordingPath() const;
  bool writeWavHeader(File& file, uint32_t dataBytes);
  bool writeRecordChunk(size_t bufferIndex);
  void serviceRecording();
  void finishRecording(bool keepFile);

  bool _sdAvailable = false;
  bool _libraryLoaded = false;
  std::atomic<uint32_t> _trackCatalogueRevision{2}, _stationCatalogueRevision{2};
  String _selectedRadioUrl;
  bool _stationsLoaded = false;
  size_t _scanEntries = 0;
  uint8_t _volume = 150;
  Tab5AudioMode _mode = Tab5AudioMode::Idle;
  bool _paused = false;
  bool _uiDirty = true;
  String _nowPlaying;
  String _status;
  String _lastRecordingPath;
  String _currentLocalPath;
  std::vector<String> _tracks;
  std::vector<Tab5AudioStation> _stations;
  size_t _trackIndex = 0;
  size_t _stationIndex = 0;
  String _currentAlbumFolder;
  String _stationCategory;

  String _wifiSsid;
  String _wifiPassword;
  std::vector<Tab5WifiNetwork> _wifiNetworks;
  String _wifiScanStatus;
  std::atomic<bool> _wifiPowerSave{true};
  bool _wifiSleeping = false;
  uint32_t _networkLastBusy = 0;
  uint32_t _wifiPowerAttemptAt = 0;
  bool _wifiPending = false;
  bool _wifiEnabled = false;
  bool _wifiAutoStart = false;
  bool _wifiScanQueued = false;  // Protected by StateGuard, including queue reset.
  std::atomic<bool> _wifiScanRunning{false};
  bool _saveWifiOnConnect = false;
  bool _playRadioAfterWifi = false;
  uint32_t _wifiScanRevision = 0;
  uint32_t _wifiStartedAt = 0;
  bool _radioReconnectPending = false;
  uint32_t _radioReconnectAt = 0;
  uint8_t _radioRetryCount = 0;
  bool _radioStartCancelled = false;

  AudioOutputTab5Speaker* _output = nullptr;
  AudioFileSourceSD* _localSource = nullptr;
  AudioFileSource* _streamSource = nullptr;
  // Meme objet que _streamSource lorsqu'une radio joue, garde typé pour
  // lire les compteurs de la file circulaire dans la télémétrie.
  Tab5BufferedRadioSource* _radioSource = nullptr;
  Tab5NetworkLease* _radioLease = nullptr;
  AudioGeneratorMP3* _mp3 = nullptr;
  AudioGeneratorAAC* _aac = nullptr;
  AudioGeneratorWAV* _wav = nullptr;
  AudioGeneratorFLAC* _flac = nullptr;
  void* _aacCodecBuffer = nullptr;
  void* _mp3CodecBuffer = nullptr;
  size_t _mp3CodecBufferSize = 0;
  bool _playingRecording = false;
  uint32_t _playStartedAt = 0;
  uint32_t _pauseStartedAt = 0;
  uint32_t _localEstimatedSeconds = 0;
  mutable SemaphoreHandle_t _stateMutex = nullptr;
  mutable SemaphoreHandle_t _snapshotMutex = nullptr;
  Tab5AudioSnapshot _snapshot;
  std::atomic<uint32_t> _indicatorRevision{1};
  std::atomic<uint32_t> _mediaRevision{1};
  bool _snapshotDirty = true;
  uint32_t _lastSnapshotAt = 0;
  uint32_t _lastWifiSignalAt = 0;
  int32_t _wifiRssiCached = 0;
  bool _wifiSignalSampled = false;
  std::atomic<bool> _wifiSignalRequested{false};
  uint32_t _lastTelemetryAt = 0;
  std::atomic<TaskHandle_t> _audioTask{nullptr};
  Tab5AudioWorkerLifecycle _workerLifecycle;
  std::atomic<bool> _idleReleaseRequested{false};
  uint32_t _lastHousekeepingAttempt = 0, _lastWorkerAttempt = 0;
  bool _workerRetryPending = false;
  QueueHandle_t _commandQueue = nullptr;
  StaticQueue_t _commandQueueControl{};
  uint8_t* _commandStorage = nullptr;
  Tab5AudioPriorityControl _memoryPriority;
  bool _priorityResume = false;
  bool _priorityWasPaused = false;
  bool _priorityWasRadio = false;
  String _priorityPath;
  uint16_t _priorityPosition = 0;
  Tab5AudioCommand _priorityCommand{};
  bool _priorityCommandPending = false;
  // Cœur portant LVGL et le réseau, et cœur réservé au décodage et au
  // haut-parleur M5Unified. Déterminés une fois dans begin().
  BaseType_t _uiCore = 0;
  BaseType_t _audioCore = 0;

  File _recordFile;
  String _recordPath;
  static constexpr uint32_t RECORD_SAMPLE_RATE = 16000;
  static constexpr uint8_t RECORD_MIC_MAGNIFICATION = 32;
  static constexpr size_t RECORD_SAMPLES = 1024;
  int16_t (*_recordBuffer)[RECORD_SAMPLES] = nullptr;
  uint8_t _recordQueueIndex = 0;
  uint8_t _recordPendingCount = 0;
  uint32_t _recordDataBytes = 0;
  uint32_t _recordStartedAt = 0;
  uint16_t _recordLevel = 0;
};

extern Tab5AudioEngine Tab5Audio;

// Declare before printer network/client scopes; destruction then resumes audio
// only after the print sockets, TLS workspace and task working buffers close.
class Tab5AudioPriorityLease {
 public:
  ~Tab5AudioPriorityLease() { release(); }
  Tab5AudioPriorityLease() = default;
  Tab5AudioPriorityLease(const Tab5AudioPriorityLease&) = delete;
  Tab5AudioPriorityLease& operator=(const Tab5AudioPriorityLease&) = delete;
  bool tryAcquire(uint32_t timeoutMs = 7000) {
    if (!_token) _token = Tab5Audio.acquireMemoryPriority(timeoutMs);
    return _token != 0;
  }
  explicit operator bool() const { return _token != 0; }
  void release() {
    if (_token) Tab5Audio.releaseMemoryPriority(_token);
    _token = 0;
  }
 private:
  uint32_t _token = 0;
};
