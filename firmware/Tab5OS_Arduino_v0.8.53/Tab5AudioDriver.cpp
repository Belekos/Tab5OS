#include "Tab5Serial.h"
#include "Tab5AudioDriver.h"

#include <algorithm>
#include <cstring>
#include <esp_heap_caps.h>

#ifndef TAB5_SPEAKER_LONG_DMA
#define TAB5_SPEAKER_LONG_DMA 0
#endif

#ifndef TAB5_SPEAKER_FORCE_SAMPLE_RATE
#define TAB5_SPEAKER_FORCE_SAMPLE_RATE 1
#endif

#ifndef TAB5_AUDIO_DRIVER_TRACE
#define TAB5_AUDIO_DRIVER_TRACE 1
#endif

#if TAB5_AUDIO_DRIVER_TRACE
#define DRIVER_LOG(...) Tab5DebugSerial.printf(__VA_ARGS__)
#else
#define DRIVER_LOG(...) do {} while (0)
#endif

Tab5AudioDriver Tab5AudioDevice;

namespace {
class RoutingGuard {
 public:
  RoutingGuard(SemaphoreHandle_t mutex, uint32_t timeoutMs)
      : mutex_(mutex), held_(mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE) {}
  ~RoutingGuard() { if (held_) xSemaphoreGive(mutex_); }
  explicit operator bool() const { return held_; }
 private:
  SemaphoreHandle_t mutex_;
  bool held_;
};
}

bool Tab5AudioDriver::begin(uint8_t volume, BaseType_t audioCore,
                            uint8_t taskPriority) {
  if (!_ownerMutex) _ownerMutex = xSemaphoreCreateMutex();
  if (!_ownerMutex) return false;

  _volume = volume;
  _audioCore = audioCore;
  _taskPriority = taskPriority;

  // Configuration changes require the previous Speaker worker to be joined.
  const bool wasRunning = M5.Speaker.isRunning();
  if (wasRunning) M5.Speaker.end();

  auto cfg = M5.Speaker.config();
#if TAB5_SPEAKER_FORCE_SAMPLE_RATE
  cfg.sample_rate = 48000;
#endif
#if TAB5_SPEAKER_LONG_DMA
  cfg.dma_buf_len = 512;
  cfg.dma_buf_count = 12;
#endif
  cfg.task_priority = _taskPriority;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#ifndef M5UNIFIED_HAS_CLOCKED_ENABLE
#error "Tab5OS requires the isolated M5Unified audio startup patch; use the supplied build scripts"
#endif
  cfg.enable_after_i2s = true;
#endif
#if !(defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE)
  cfg.task_pinned_core = static_cast<uint8_t>(_audioCore);
#endif
  M5.Speaker.config(cfg);

  // Configure one stable output clock. I2S/DMA are allocated when a client
  // claims playback; radio admission deliberately claims these before TCP.

  _spec.sampleRate = M5.Speaker.config().sample_rate;
  _spec.channels = 2;
  _spec.format = Tab5PcmFormat::S16;
  _begun = true;
  _suspended = false;
  _outputFailed = false;
  M5.Speaker.setVolume(_volume);
  // Jack state is read when a client starts playback, not while audio is idle.
  clearBlocks();

  const auto applied = M5.Speaker.config();
  DRIVER_LOG(
      "[audio-driver] a la demande : %u Hz, DMA %ux%u, coeur %u, priorite %u, sortie %s\n",
      (unsigned)applied.sample_rate, (unsigned)applied.dma_buf_len,
      (unsigned)applied.dma_buf_count, (unsigned)applied.task_pinned_core,
      (unsigned)applied.task_priority,
      _headphoneInserted ? "jack" : "haut-parleur");
  return true;
}

bool Tab5AudioDriver::claim(Tab5AudioClient client, uint32_t timeoutMs) {
  if (client == Tab5AudioClient::None || !_ownerMutex) return false;
  const uint32_t started = millis();
  while (true) {
    if (xSemaphoreTake(_ownerMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      const bool accepted = _owner == Tab5AudioClient::None || _owner == client;
      if (accepted) {
        if (!_buffer) {
          _buffer = static_cast<int16_t (*)[BUFFER_SAMPLES]>(heap_caps_calloc(
              BUFFER_SLOTS, BUFFER_SAMPLES * sizeof(int16_t),
              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
          if (!_buffer) { xSemaphoreGive(_ownerMutex); return false; }
        }
        if (_owner != client) {
          clearBlocks();
          _outputFailed = false;
        }
        _owner = client;
      }
      xSemaphoreGive(_ownerMutex);
      if (accepted) {
        if (ensureSpeakerRunning()) return true;
        release(client);
        return false;
      }
    }
    if (timeoutMs != UINT32_MAX && millis() - started >= timeoutMs) return false;
    vTaskDelay(1);
  }
}

bool Tab5AudioDriver::release(Tab5AudioClient client) {
  if (client == Tab5AudioClient::None || !_ownerMutex) return false;
  // A release is a lifetime barrier, never a best-effort routing request.
  // Only the owning worker calls this; the UI uses nonblocking service().
  if (xSemaphoreTake(_ownerMutex, portMAX_DELAY) != pdTRUE) return false;
  if (_owner == client) {
    if (M5.Speaker.isRunning()) M5.Speaker.end();
    heap_caps_free(_buffer);
    _buffer = nullptr;
    clearBlocks();
    _owner = Tab5AudioClient::None;
  }
  xSemaphoreGive(_ownerMutex);
  return true;
}

bool Tab5AudioDriver::ensureSpeakerRunning() {
  if (!_begun || _suspended || _outputFailed) return false;
  if (M5.Speaker.isRunning()) return true;
  DRIVER_LOG("[audio-driver] DEMARRAGE_I2S\n");
  // A failed codec/clock/prime operation is reported to its caller. Repeating
  // the same power sequence with a different DMA size cannot repair I2C.
  if (!M5.Speaker.begin()) {
    _outputFailed = true;
    return false;
  }
  M5.Speaker.setVolume(_volume);
  refreshHeadphoneRouting();
  DRIVER_LOG("[audio-driver] I2S_PRET\n");
  return true;
}

bool Tab5AudioDriver::configure(Tab5AudioClient client,
                                const Tab5PcmSpec& spec) {
  if (client == Tab5AudioClient::None || _owner != client || !_buffer || _outputFailed ||
      !spec.sampleRate || (spec.channels != 1 && spec.channels != 2) ||
      static_cast<uint8_t>(spec.format) > static_cast<uint8_t>(Tab5PcmFormat::S32))
    return false;
  if (_spec.sampleRate != spec.sampleRate && !flush(client)) return false;
  // M5Unified accepts each input rate in playRaw and resamples to its fixed
  // I2S output clock. Restarting I2S here power-cycled the codec/amplifier on
  // MP3's first decoded frame (48 kHz -> 44.1 kHz), and on every rate change.
  // Flush under the previous input rate before publishing the next one.
  _spec = spec;
  return true;
}

bool Tab5AudioDriver::consumeStereo16(Tab5AudioClient client,
                                      int16_t left, int16_t right) {
  if (client == Tab5AudioClient::None || _owner != client || !_buffer ||
      _suspended || _outputFailed) return false;
  if (_bufferIndex + 2 > BUFFER_SAMPLES) {
    pushCurrentBlock();
    return false;
  }
  int16_t* slot = _buffer[_bufferSlot];
  slot[_bufferIndex++] = left;
  slot[_bufferIndex++] = right;
  return true;
}

bool Tab5AudioDriver::pushCurrentBlock() {
  if (_outputFailed) return false;
  if (!_bufferIndex) return true;
  if (!ensureSpeakerRunning()) return false;
  int16_t* slot = _buffer[_bufferSlot];
  const size_t length = _bufferIndex;
  // M5Unified waits on its native semaphore for a busy explicit channel.
  // A false result means no request was accepted, not temporary queue pressure.
  const uint32_t started = millis();
  const bool submitted = M5.Speaker.playRaw(slot, length, _spec.sampleRate,
                                           true, 1, SPEAKER_CHANNEL);
  _waitTicks += pdMS_TO_TICKS(static_cast<uint32_t>(millis() - started));
  if (submitted) {
    _pcmQuiescent = false;
    ++_pushedBlocks;
    _bufferSlot = (_bufferSlot + 1) % BUFFER_SLOTS;
    _bufferIndex = 0;
    return true;
  }
  ++_droppedBlocks;
  _outputFailed = true;
  return false;
}

bool Tab5AudioDriver::flush(Tab5AudioClient client) {
  return client != Tab5AudioClient::None && _owner == client && _buffer &&
         pushCurrentBlock();
}

bool Tab5AudioDriver::writePcm(Tab5AudioClient client, const void* data,
                               size_t frames, const Tab5PcmSpec& spec) {
  if (!data || !frames || _owner != client || !configure(client, spec))
    return false;
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  const size_t bytesPerSample = spec.format == Tab5PcmFormat::U8 ? 1U
      : (spec.format == Tab5PcmFormat::S16 ? 2U
      : (spec.format == Tab5PcmFormat::S24 ? 3U : 4U));
  if (frames > SIZE_MAX / (spec.channels * bytesPerSample)) return false;
  auto sampleAt = [&](size_t index) -> int16_t {
    const uint8_t* p = bytes + index * bytesPerSample;
    switch (spec.format) {
      case Tab5PcmFormat::U8:
        // Multiplication is defined for negative values; left-shifting a
        // negative signed sample is undefined in pre-C++20 implementations.
        return static_cast<int16_t>((static_cast<int32_t>(p[0]) - 128) * 256);
      case Tab5PcmFormat::S16:
        return static_cast<int16_t>(static_cast<uint16_t>(p[0]) |
                                    (static_cast<uint16_t>(p[1]) << 8));
      case Tab5PcmFormat::S24: {
        int32_t value = static_cast<int32_t>(p[0]) |
                        (static_cast<int32_t>(p[1]) << 8) |
                        (static_cast<int32_t>(p[2]) << 16);
        if (value & 0x00800000) value |= static_cast<int32_t>(0xff000000);
        return static_cast<int16_t>(value >> 8);
      }
      case Tab5PcmFormat::S32: {
        const int32_t value = static_cast<int32_t>(
            static_cast<uint32_t>(p[0]) |
            (static_cast<uint32_t>(p[1]) << 8) |
            (static_cast<uint32_t>(p[2]) << 16) |
            (static_cast<uint32_t>(p[3]) << 24));
        return static_cast<int16_t>(value >> 16);
      }
    }
    return 0;
  };

  for (size_t frame = 0; frame < frames; ++frame) {
    const size_t base = frame * spec.channels;
    const int16_t left = sampleAt(base);
    const int16_t right = spec.channels == 2 ? sampleAt(base + 1) : left;
    while (!consumeStereo16(client, left, right)) {
      if (_owner != client || _suspended || _outputFailed) return false;
    }
  }
  return true;
}

void Tab5AudioDriver::stop(Tab5AudioClient client) {
  if (client == Tab5AudioClient::None || _owner != client) return;
  // Native worker acknowledgement retires every PCM reference. Pause and
  // seek need this barrier, not a power cycle of the codec and amplifier.
  if (!_pcmQuiescent && M5.Speaker.isRunning() &&
      !M5.Speaker.stopAndWait(SPEAKER_CHANNEL)) {
    _outputFailed = true;
    return;
  }
  clearBlocks();
}

void Tab5AudioDriver::clearBlocks() {
  _pcmQuiescent = true;
  _bufferSlot = 0;
  _bufferIndex = 0;
  _droppedBlocks = 0;
  _pushedBlocks = 0;
  _waitTicks = 0;
  // Only initialized samples up to _bufferIndex are ever submitted. Reset
  // ownership/lengths after quiescence; rewriting 12 KiB of PSRAM is needless.
}

void Tab5AudioDriver::suspend() {
  if (!_begun || _suspended) return;
  _bufferIndex = 0;
  M5.Speaker.stop(SPEAKER_CHANNEL);
  if (M5.Speaker.isRunning()) M5.Speaker.end();
  _suspended = true;
}

bool Tab5AudioDriver::resume() {
  if (!_begun) return false;
  _suspended = false;
  return _owner == Tab5AudioClient::None || ensureSpeakerRunning();
}

void Tab5AudioDriver::setVolume(uint8_t volume) {
  _volume = volume;
  M5.Speaker.setVolume(volume);
}

bool Tab5AudioDriver::readHeadphoneDetected(bool& inserted) {
  uint8_t value = 0;
  if (!M5.getIOExpander(0).readRegister(0x0F, &value, 1)) {
    ++_headphoneReadErrors;
    return false;
  }
  inserted = (value & (1U << TAB5_HP_DET_PIN)) != 0;
  return true;
}

void Tab5AudioDriver::applyHeadphoneRouting(bool inserted) {
  const bool ok = M5.getIOExpander(0).digitalWrite(
      TAB5_SPK_EN_PIN, M5.Speaker.isRunning() && !inserted);
  _headphoneRoutePending = !ok;
  if (!ok) ++_headphoneRouteErrors;
}

void Tab5AudioDriver::refreshHeadphoneRouting() {
  RoutingGuard guard(_ownerMutex, 250);
  if (!guard) { _headphoneRoutePending = true; return; }
  bool inserted;
  if (readHeadphoneDetected(inserted)) {
    _headphoneInserted = inserted;
    _headphoneStateKnown = true;
  }
  // Speaker.begin() changes SPK_EN. Restore the last confirmed route; if no
  // read has succeeded yet, keep the loudspeaker muted until one does.
  applyHeadphoneRouting(!_headphoneStateKnown || _headphoneInserted);
}

void Tab5AudioDriver::service() {
  if (!_begun || _owner == Tab5AudioClient::None || _suspended ||
      !M5.Speaker.isRunning()) return;
  const uint32_t now = millis();
  if (now - _lastHeadphonePollAt < HEADPHONE_POLL_MS) return;
  _lastHeadphonePollAt = now;
  RoutingGuard guard(_ownerMutex, 0);
  if (!guard || _owner == Tab5AudioClient::None || _suspended ||
      !M5.Speaker.isRunning()) return;
  bool inserted;
  if (!readHeadphoneDetected(inserted)) return;
  const bool changed = !_headphoneStateKnown || inserted != _headphoneInserted;
  if (!changed && !_headphoneRoutePending) return;
  _headphoneInserted = inserted;
  _headphoneStateKnown = true;
  applyHeadphoneRouting(inserted);
  if (changed) DRIVER_LOG("[audio-driver] sortie : %s\n",
      inserted ? "casque 3,5 mm" : "haut-parleur interne");
}

AudioOutputTab5Speaker::AudioOutputTab5Speaker(Tab5AudioDriver* driver)
    : _driver(driver) {
  // AudioOutput ne garantit pas que le codec appellera SetChannels avant
  // SetRate. Des valeurs déterministes évitent donc de configurer le pilote
  // commun avec un nombre de canaux indéfini.
  hertz = 48000;
  channels = 2;
  gainF2P6 = 1U << 6;
  _spec.sampleRate = 48000;
  _spec.channels = 2;
  _spec.format = Tab5PcmFormat::S16;
}

bool AudioOutputTab5Speaker::begin() {
  if (!_driver || !_driver->claim(Tab5AudioClient::Audio, 250)) return false;
  if (_driver->configure(Tab5AudioClient::Audio, _spec)) return true;
  _driver->release(Tab5AudioClient::Audio);
  return false;
}

bool AudioOutputTab5Speaker::ConsumeSample(int16_t sample[2]) {
  return _driver && sample && _driver->consumeStereo16(
      Tab5AudioClient::Audio, sample[0], sample[1]);
}

bool AudioOutputTab5Speaker::SetRate(int hz) {
  if (!_driver || hz <= 0) return false;
  hertz = hz;
  _spec.sampleRate = static_cast<uint32_t>(hz);
  if (_driver->owner() != Tab5AudioClient::Audio) return true;
  return _driver->configure(Tab5AudioClient::Audio, _spec);
}

bool AudioOutputTab5Speaker::SetChannels(int value) {
  if (value != 1 && value != 2) return false;
  channels = static_cast<uint8_t>(value);
  _spec.channels = channels;
  if (!_driver || _driver->owner() != Tab5AudioClient::Audio) return true;
  return _driver->configure(Tab5AudioClient::Audio, _spec);
}

void AudioOutputTab5Speaker::flush() {
  if (_driver) _driver->flush(Tab5AudioClient::Audio);
}

bool AudioOutputTab5Speaker::stop() {
  if (!_driver) return false;
  _driver->stop(Tab5AudioClient::Audio);
  return true;
}

void AudioOutputTab5Speaker::setMasterVolume(uint8_t value) {
  if (_driver) _driver->setVolume(value);
}

uint32_t AudioOutputTab5Speaker::droppedBlocks() const {
  return _driver ? _driver->droppedBlocks() : 0;
}

uint32_t AudioOutputTab5Speaker::pushedBlocks() const {
  return _driver ? _driver->pushedBlocks() : 0;
}

uint32_t AudioOutputTab5Speaker::waitTicks() const {
  return _driver ? _driver->waitTicks() : 0;
}

int AudioOutputTab5Speaker::sourceRate() const {
  return _driver ? static_cast<int>(_driver->sourceRate()) : 0;
}

size_t AudioOutputTab5Speaker::blockSamples() const {
  return _driver ? _driver->blockSamples() : 0;
}
