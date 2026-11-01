#pragma once
#include <atomic>

#include <Arduino.h>
#include <FS.h>
#include <AudioOutput.h>
#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Tous les producteurs PCM de Tab5OS passent par ce pilote. Lui seul pilote
// M5.Speaker, l'I2S de l'ESP32-P4, le volume et la commutation jack/HP.
enum class Tab5AudioClient : uint8_t {
  None,
  Audio,
  Video,
  Native,
  Alarm,
  Sdr,
};

enum class Tab5PcmFormat : uint8_t {
  U8,
  S16,
  S24,
  S32,
};

struct Tab5PcmSpec {
  uint32_t sampleRate = 48000;
  uint8_t channels = 2;
  Tab5PcmFormat format = Tab5PcmFormat::S16;
};

class Tab5AudioDriver {
 public:
  // A client's configure/write/stop/release calls belong to its one owning
  // worker. UI controls send commands to that worker, never PCM pointers.
  bool begin(uint8_t volume, BaseType_t audioCore, uint8_t taskPriority);
  void service();

  bool claim(Tab5AudioClient client, uint32_t timeoutMs = 0);
  // Returns only after PCM references and the final transport are released.
  bool release(Tab5AudioClient client);
  Tab5AudioClient owner() const { return _owner; }
  // A terminal sink error belongs to the current stream, not back-pressure.
  // It remains observable through stop/release and resets on a new claim.
  bool failed() const { return _outputFailed.load(); }

  bool configure(Tab5AudioClient client, const Tab5PcmSpec& spec);
  bool consumeStereo16(Tab5AudioClient client, int16_t left, int16_t right);
  bool writePcm(Tab5AudioClient client, const void* data, size_t frames,
                const Tab5PcmSpec& spec);
  bool flush(Tab5AudioClient client);
  // Quiesce PCM while retaining clocks for pause/seek/source transitions.
  void stop(Tab5AudioClient client);

  void suspend();
  bool resume();
  void setVolume(uint8_t volume);
  uint8_t volume() const { return _volume; }

  uint32_t droppedBlocks() const { return _droppedBlocks; }
  uint32_t pushedBlocks() const { return _pushedBlocks; }
  uint32_t waitTicks() const { return _waitTicks; }
  uint32_t sourceRate() const { return _spec.sampleRate; }
  uint32_t outputRate() const {
    return M5.Speaker.isRunning() ? M5.Speaker.config().sample_rate : 0;
  }
  size_t blockSamples() const { return BUFFER_SAMPLES; }
  bool headphonesConnected() const { return _headphoneInserted; }
  uint32_t headphoneReadErrors() const { return _headphoneReadErrors; }
  uint32_t headphoneRouteErrors() const { return _headphoneRouteErrors; }

 private:
  bool ensureSpeakerRunning();
  bool pushCurrentBlock();
  void clearBlocks();
  bool readHeadphoneDetected(bool& inserted);
  void refreshHeadphoneRouting();
  void applyHeadphoneRouting(bool inserted);

  static constexpr uint8_t SPEAKER_CHANNEL = 0;
  static constexpr uint8_t TAB5_SPK_EN_PIN = 1;
  static constexpr uint8_t TAB5_HP_DET_PIN = 7;
  static constexpr uint32_t HEADPHONE_POLL_MS = 200;
  static constexpr size_t BUFFER_SAMPLES = 2048;
  static constexpr size_t BUFFER_SLOTS = 3;

  std::atomic<Tab5AudioClient> _owner{Tab5AudioClient::None};
  SemaphoreHandle_t _ownerMutex = nullptr;
  Tab5PcmSpec _spec;
  std::atomic<uint8_t> _volume{150};
  BaseType_t _audioCore = 0;
  uint8_t _taskPriority = 6;
  bool _begun = false;
  std::atomic<bool> _suspended{false};
  std::atomic<bool> _outputFailed{false};

  int16_t (*_buffer)[BUFFER_SAMPLES] = nullptr;
  size_t _bufferSlot = 0;
  size_t _bufferIndex = 0;
  bool _pcmQuiescent = true;
  uint32_t _droppedBlocks = 0;
  uint32_t _pushedBlocks = 0;
  uint32_t _waitTicks = 0;

  uint32_t _lastHeadphonePollAt = 0;
  std::atomic<bool> _headphoneStateKnown{false};
  std::atomic<bool> _headphoneRoutePending{false};
  std::atomic<uint32_t> _headphoneReadErrors{0};
  std::atomic<uint32_t> _headphoneRouteErrors{0};
  std::atomic<bool> _headphoneInserted{false};
};

// Adaptateur ESP8266Audio. Il ne connaît ni l'I2S ni le codec : il transforme
// seulement l'API échantillon-par-échantillon en appels au pilote commun.
class AudioOutputTab5Speaker : public AudioOutput {
 public:
  explicit AudioOutputTab5Speaker(Tab5AudioDriver* driver);

  bool begin() override;
  bool ConsumeSample(int16_t sample[2]) override;
  bool SetRate(int hz) override;
  bool SetChannels(int channels) override;
  void flush() override;
  bool stop() override;

  void setMasterVolume(uint8_t value);
  uint32_t droppedBlocks() const;
  uint32_t pushedBlocks() const;
  uint32_t waitTicks() const;
  int sourceRate() const;
  size_t blockSamples() const;

 private:
  Tab5AudioDriver* _driver = nullptr;
  Tab5PcmSpec _spec;
};

extern Tab5AudioDriver Tab5AudioDevice;
