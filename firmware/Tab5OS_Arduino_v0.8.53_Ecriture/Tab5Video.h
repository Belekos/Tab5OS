#pragma once
#include "Tab5CatalogueRevision.h"
#include <atomic>

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <vector>

struct Tab5VideoItem {
  String name;
  String source;
  String format;
  float fps = 25.0f;
  bool remote = false;
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

struct Tab5VideoFrame {
  const uint8_t* data = nullptr;
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t stride = 0;
  uint32_t generation = 0;
};

class Tab5VideoEngine {
 public:
  bool begin(bool sdAvailable, BaseType_t taskCore);
  // UI task only; refreshes after an exclusive SD remount, never scans at boot.
  bool setStorageAvailable(bool available);
  void ensureLibraryLoaded();
  void reload();
  size_t reclaimIdle();

  uint32_t catalogueRevision() const { return tab5CatalogueRevision(_catalogueRevision); }
  size_t count() const;
  bool item(size_t index, Tab5VideoItem& result) const;
  bool play(size_t index);
  bool playSource(const String& source, const String& name = String(),
                  float fps = 25.0f, const String& format = String());
  void togglePause();
  // UI task only. Stop requests cancellation. A running worker consumes the
  // latest queued replacement after closing its resources; service handles
  // requests queued just as that worker retires.
  // A true playSource return means accepted, not that decoding has succeeded.
  void stop();
  void service();
  // UI must detach the LVGL image before calling this. A still-closing
  // worker releases the frames itself after its last decode has finished.
  void detachDisplay();

  bool snapshot(Tab5VideoSnapshot& result) const;
  bool frame(Tab5VideoFrame& result);
  bool takeUiDirty();

 private:
  static void taskEntry(void* context);
  void taskMain();
  void preparePlaybackLocked(Tab5VideoItem next);
  void releaseFrames();
  void trimIdleFrames(); // UI only, with _mutex held; no producer can own a frame.
  bool playAvi(class Tab5VideoInput& input, class Tab5AudioPriorityLease& priority);
  bool playMjpeg(class Tab5VideoInput& input, float fps);
  bool playMultipartMjpeg(class Tab5VideoInput& input);
  bool playRawMjpeg(class Tab5VideoInput& input, float fps);
  bool decodeAndPublish(const uint8_t* jpeg, size_t length,
                        uint64_t presentationUs);
  bool waitUntil(uint64_t presentationUs);
  bool ensureJpegInput(size_t required, size_t preserveBytes = 0);
  bool ensureDecoderBuffers(size_t requiredOutput);
  void releaseDecoderResources();
  void logMemory(const char* stage) const;
  void releasePlayback();
  void setState(Tab5VideoState state, const String& status);
  void scanFolder(const String& folder, uint8_t depth);
  void loadRemoteCatalog();

  mutable SemaphoreHandle_t _mutex = nullptr;
  std::vector<Tab5VideoItem> _items;
  bool _sdAvailable = false;
  bool _catalogLoaded = false;
  std::atomic<uint32_t> _catalogueRevision{2};
  size_t _scanEntries = 0;
  BaseType_t _taskCore = 0;
  TaskHandle_t _task = nullptr;
  std::atomic<bool> _taskRunning{false};
  std::atomic<bool> _stopRequested{false};
  std::atomic<bool> _paused{false};
  std::atomic<bool> _uiDirty{true};

  Tab5VideoSnapshot _state;
  String _pendingSource;
  String _pendingName;
  String _pendingFormat;
  float _pendingFps = 25.0f;
  // Separate from the active task's startup parameters: that task may not
  // have copied them yet when UI requests its replacement.
  Tab5VideoItem _queuedPlay;
  bool _playQueued = false; // guarded by _mutex; UI enqueues, worker/service consume
  uint64_t _clockStartedUs = 0;
  uint64_t _pauseStartedUs = 0;

  void* _jpegDecoder = nullptr;
  uint8_t* _jpegInput = nullptr;
  size_t _jpegInputCapacity = 0;
  uint8_t* _frames[3] = {nullptr, nullptr, nullptr};
  size_t _frameCapacity = 0;
  uint8_t _frontFrame = 0;
  uint8_t _uiFrame = 0xff;
  bool _displayAttached = false; // guarded by _mutex
  uint16_t _frameStride = 0;
  uint32_t _frameGeneration = 0;
  uint8_t _consecutiveDecodeErrors = 0;
};

extern Tab5VideoEngine Tab5Video;
