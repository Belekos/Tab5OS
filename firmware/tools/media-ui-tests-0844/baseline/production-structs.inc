enum class Tab5AudioMode : uint8_t {
  Idle,
  Local,
  Radio,
  Recording,
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
enum class Tab5VideoState : uint8_t {
  Idle,
  Opening,
  Playing,
  Paused,
  Finished,
  Error,
};
struct Tab5VideoSnapshot {
  Tab5VideoState state = Tab5VideoState::Idle;
  String name;
  String status;
  uint32_t elapsedMs = 0;
  uint32_t frameNumber = 0;
  uint32_t droppedFrames = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  bool hasAudio = false;
  bool remote = false;
};