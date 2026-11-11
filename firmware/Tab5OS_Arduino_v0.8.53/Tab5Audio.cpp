#include "Tab5Serial.h"
#include "Tab5SecureClient.h"
#include "Tab5Compatibility.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5FileShare.h"
#include "Tab5Audio.h"
#include "Tab5MediaSession.h"
#include "Tab5ResourceManager.h"
#include "Tab5Task.h"
#include "Tab5ConnectionPolicy.h"
#include "Tab5NetDiagnostics.h"
#include "Tab5Memory.h"
#include "Tab5NetworkGate.h"
#include "Tab5CooperativeClient.h"
#include "Tab5RadioBodyReader.h"
#include "Tab5RadioCodec.h"
#include <atomic>
#include <new>
#include <optional>

#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#ifndef ARDUINOJSON_ENABLE_STD_STREAM
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#endif
#ifndef ARDUINOJSON_ENABLE_STD_STRING
#define ARDUINOJSON_ENABLE_STD_STRING 0
#endif
#include "Tab5Json.h"
#include <algorithm>
#include <strings.h>
#include <cerrno>
#include <time.h>
#include <esp_heap_caps.h>
#include <freertos/stream_buffer.h>

// ESP8266Audio decodes the compressed stream. The final PCM samples are not
// sent directly to I2S: M5Unified owns the Tab5 codec and receives them here.
#include <AudioFileSource.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorAAC.h>
#include <libhelix-aac/coder.h>
// Helix's coder.h and sbr.h reuse private typedef names; query SBR sizing
// from a separate translation unit instead of mixing those private headers.
size_t tab5AacSbrWorkspaceBytes();
#include <AudioGeneratorWAV.h>
#include <AudioGeneratorFLAC.h>

// Demander les metadonnees ICY (titre de la piste). A 0 : le serveur envoie
// du MP3 pur et toute une classe de desynchronisation disparait.
#ifndef TAB5_RADIO_REQUEST_ICY
#define TAB5_RADIO_REQUEST_ICY 0
#endif

// Traces serie a l'ouverture d'un flux radio, avec l'etat des tas.
#ifndef TAB5_RADIO_TRACE
#define TAB5_RADIO_TRACE 1
#endif

#if TAB5_RADIO_TRACE
#define TAB5_LOG(...) Tab5DebugSerial.printf(__VA_ARGS__)
static void tab5LogHeap(const char* stage) {
  Tab5DebugSerial.printf("[radio] %s | interne libre %u Kio (bloc max %u Kio) | PSRAM libre %u Kio\n",
                stage,
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}
#else
#define TAB5_LOG(...) do {} while (0)
static void tab5LogHeap(const char*) {}
#endif

static constexpr char AUDIO_ROOT[] = "/Audio";
static constexpr char LEGACY_MP3_ROOT[] = "/mp3";
static constexpr char RECORD_ROOT[] = "/Recordings";
static constexpr char RADIO_FILE[] = "/radios.json";
static constexpr char LEGACY_RADIO_FILE[] = "/System/radios.json";
static constexpr char WIFI_PREFS_NAMESPACE[] = "tab5os-net";
static constexpr size_t MAX_AUDIO_ITEMS = 800;
static constexpr uint8_t MAX_AUDIO_DEPTH = 8;
static constexpr size_t MAX_AUDIO_SCAN_ENTRIES = 4096;
static constexpr size_t MAX_RADIO_CATALOGUE_BYTES = 256U * 1024U;
static constexpr size_t MAX_RADIO_STATIONS = 800;
static constexpr uint32_t WIFI_TIMEOUT_MS = 12000;
// Cœur portant LVGL et le réseau, et cœur réservé à la chaîne audio. Les deux
// valeurs sont renseignées par Tab5AudioEngine::begin(). Le producteur réseau
// est volontairement placé à côté de LVGL : il passe l'essentiel de son temps
// bloqué sur la socket, alors que le décodeur doit garder son cœur pour lui.
static BaseType_t g_networkTaskCore = tskNO_AFFINITY;
// File de commandes du moteur, publiee ici pour que les attentes longues
// du flux radio (prechargement, reconstitution d'avance) rendent la main
// des qu'une action utilisateur est en attente. Sans cela, un appui sur
// Stop pouvait rester sans effet visible plusieurs secondes.
static QueueHandle_t g_audioCommandQueue = nullptr;
static Tab5AudioPriorityControl* g_audioMemoryPriority = nullptr;
static bool audioCommandPending() {
  return (g_audioCommandQueue && uxQueueMessagesWaiting(g_audioCommandQueue) > 0) ||
      (g_audioMemoryPriority && g_audioMemoryPriority->state() ==
                                   Tab5AudioPriorityControl::Requested);
}
static constexpr UBaseType_t NETWORK_TASK_PRIORITY = 4;
static constexpr UBaseType_t DECODER_TASK_PRIORITY = 5;
static constexpr UBaseType_t SPEAKER_TASK_PRIORITY = 6;

// ESP8266Audio sait déjà resynchroniser libmad après un déplacement, mais son
// API publique ne remet pas la trame PCM courante à zéro. Cette petite classe
// ne change pas le décodage : elle expose seulement un déplacement atomique
// dans la source SD, suivi de la resynchronisation prévue par la bibliothèque.
class Tab5SeekableMp3Generator : public AudioGeneratorMP3 {
 public:
  Tab5SeekableMp3Generator(void* space, int size)
      : AudioGeneratorMP3(space, size) {}

  bool seekToByte(uint32_t position) {
    if (!file || !file->seek(static_cast<int32_t>(position), SEEK_SET))
      return false;
    desync();
    samplePtr = 9999;
    nsCount = nsCountMax;
    lastSample[0] = 0;
    lastSample[1] = 0;
    return true;
  }
};

// The default AAC constructor spreads its workspace across malloc calls and
// begin() does not reject allocation failure. Give Helix a single PSRAM arena
// and check it before the library touches its input/output buffers.
static constexpr size_t aacAlign(size_t bytes) { return (bytes + 7U) & ~7U; }
static size_t aacWorkspaceBytes() {
  return aacAlign(1600U) + aacAlign(8192U) + aacAlign(sizeof(AACDecInfo)) +
      aacAlign(sizeof(PSInfoBase)) + tab5AacSbrWorkspaceBytes();
}
class Tab5SeekableAacGenerator : public AudioGeneratorAAC {
 public:
  Tab5SeekableAacGenerator(void* arena, size_t size)
      : AudioGeneratorAAC(arena, static_cast<int>(size)) {}

  bool begin(AudioFileSource* source, AudioOutput* target) override {
    if (outSampleLen < 4096 || !buff || !outSample || !hAACDecoder || !source || !target ||
        !source->isOpen() || !target->begin()) return false;
    return AudioGeneratorAAC::begin(source, target);
  }
  bool stop() override {
    if (!file || !output) { running = false; return true; }
    return AudioGeneratorAAC::stop();
  }
  bool seekToByte(uint32_t position) {
    if (!file || !hAACDecoder ||
        !file->seek(static_cast<int32_t>(position), SEEK_SET)) return false;
    AACFlushCodec(hAACDecoder);
    buffValid = lastFrameEnd = validSamples = curSample = 0;
    lastSample[0] = lastSample[1] = 0;
    return true;
  }
};

// Le lecteur WAV conserve le début et la taille du chunk PCM découverts par
// AudioGeneratorWAV::begin(). Un seek reste ainsi aligné sur une trame et ne
// tente jamais de relire l'en-tête comme des échantillons.
class Tab5SeekableWavGenerator : public AudioGeneratorWAV {
 public:
  bool begin(AudioFileSource* source, AudioOutput* output) override {
    if (!AudioGeneratorWAV::begin(source, output)) return false;
    _dataStart = file->getPos();
    _dataSize = availBytes;
    return _dataSize > 0;
  }

  bool seekPermille(uint16_t permille) {
    if (!file || !_dataSize || !channels || !bitsPerSample) return false;
    const uint32_t frameBytes = channels * (bitsPerSample / 8U);
    uint64_t offset = static_cast<uint64_t>(_dataSize) * permille / 1000ULL;
    if (frameBytes) offset -= offset % frameBytes;
    if (offset >= _dataSize) offset = _dataSize - (_dataSize % frameBytes);
    if (!file->seek(static_cast<int32_t>(_dataStart + offset), SEEK_SET))
      return false;
    availBytes = _dataSize - static_cast<uint32_t>(offset);
    buffPtr = 0;
    buffLen = 0;
    lastSample[0] = 0;
    lastSample[1] = 0;
    return true;
  }

  uint32_t durationSeconds() const {
    const uint32_t bytesPerSecond =
        sampleRate * channels * (bitsPerSample / 8U);
    return bytesPerSecond
        ? static_cast<uint32_t>((_dataSize + bytesPerSecond / 2U) /
                                bytesPerSecond)
        : 0;
  }

 private:
  uint32_t _dataStart = 0;
  uint32_t _dataSize = 0;
};

// libFLAC fournit nativement FLAC__stream_decoder_seek_absolute() et le
// générateur ESP8266Audio lui a déjà raccordé les callbacks seek/tell/length.
// La classe dérivée rend simplement ce mécanisme accessible à Tab5OS.
class Tab5SeekableFlacGenerator : public AudioGeneratorFLAC {
 public:
  bool begin(AudioFileSource* source, AudioOutput* target) override {
    // ESP8266Audio FLAC ignores the sink begin() result. Do not publish a
    // running decoder when the shared output could not be acquired.
    if (!source || !target || !source->isOpen() || !target->begin()) return false;
    return AudioGeneratorFLAC::begin(source, target);
  }
  bool stop() override {
    if (!file || !output) { running = false; return true; }
    return AudioGeneratorFLAC::stop();
  }
  bool seekPermille(uint16_t permille, uint32_t estimatedSeconds) {
    if (!flac) return false;
    FLAC__uint64 totalSamples = FLAC__stream_decoder_get_total_samples(flac);
    if (!totalSamples && sampleRate && estimatedSeconds) {
      totalSamples = static_cast<FLAC__uint64>(sampleRate) * estimatedSeconds;
    }
    if (!totalSamples) return false;
    FLAC__uint64 target = totalSamples * permille / 1000ULL;
    if (target >= totalSamples) target = totalSamples - 1;
    buffPtr = 0;
    buffLen = 0;
    if (!FLAC__stream_decoder_seek_absolute(flac, target)) return false;
    buffPtr = 0;
    buffLen = 0;
    lastSample[0] = 0;
    lastSample[1] = 0;
    return true;
  }
};

// HTTP sources remain explicit; HTTPS validates hostname, chain and dates.
class Tab5HttpAudioSource : public AudioFileSource {
 public:
  Tab5HttpAudioSource() = default;
  ~Tab5HttpAudioSource() override { close(); }

  void setAbortFlag(std::atomic<bool>* flag) { _abort = flag; }

  bool open(const char* url) override {
    close();
    if (!url || !url[0] || !tab5NetworkEnabled() || WiFi.status() != WL_CONNECTED) return false;
    _url = url;
    // The abort context is this long-lived source, not a stack request. During
    // playback ordinary audio commands must not tear down the transport.
    const auto cancelled = [](void* context) {
      auto* source = static_cast<Tab5HttpAudioSource*>(context);
      return source->aborted() || (!source->_opened && audioCommandPending());
    };
    NetworkClientSecure* secureClient = nullptr;
    if (_url.startsWith("https://")) {
      auto* secure = new (std::nothrow) Tab5CooperativeClient<Tab5SecureClient>(cancelled, this);
      if (!secure) { close(); return false; }
      
      secure->setHandshakeTimeout(8);
      secureClient = secure;
      _client = secure;
    } else {
      _client = new (std::nothrow) Tab5CooperativeClient<NetworkClient>(cancelled, this);
    }
    _http.emplace();
    if (!_client || !_http->begin(*_client, _url)) {
      close();
      return false;
    }
    // Prefer a plain close-delimited stream, but decode valid chunked replies
    // too. getStreamPtr() exposes their framing; it must not reach the codec.
    _http->useHTTP10(true);
    _http->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    _http->setConnectTimeout(CONNECT_TIMEOUT_MS);
    _http->setTimeout(READ_TIMEOUT_MS);
    _http->setUserAgent("VLC/3.0.20 LibVLC/3.0.20");
    static const char* headers[] = {"icy-metaint", "Transfer-Encoding", "Content-Type"};
    _http->collectHeaders(headers, 3);
    // Request plain audio by default. If a server still inserts ICY metadata,
    // readInternal strips it according to the announced byte interval.
#if TAB5_RADIO_REQUEST_ICY
    _http->addHeader("Icy-MetaData", "1");
#endif
    const int code = _http->GET();
    if (aborted() || audioCommandPending()) { close(); return false; }
    if (code < 200 || code >= 300) {
      char tlsMessage[96] = {};
      const int tlsCode = secureClient
          ? secureClient->lastError(tlsMessage, sizeof(tlsMessage)) : 0;
      const int rssi = Tab5Audio.wifiRssi();
      char signalText[20];
      if (rssi < 0) snprintf(signalText, sizeof(signalText), "%d dBm", rssi);
      else snprintf(signalText, sizeof(signalText), "n/a");
      Tab5DebugSerial.printf("[radio][NET] GET impossible : HTTP=%d (%s), "
                    "TLS=%d (%s), WiFi=%d, RSSI(cache)=%s, RAM=%u Kio, "
                    "bloc=%u Kio\n",
                    code, HTTPClient::errorToString(code).c_str(),
                    tlsCode, tlsMessage[0] ? tlsMessage : "n/a",
                    static_cast<int>(WiFi.status()), signalText,
                    static_cast<unsigned>(heap_caps_get_free_size(
                        MALLOC_CAP_INTERNAL) / 1024U),
                    static_cast<unsigned>(heap_caps_get_largest_free_block(
                        MALLOC_CAP_INTERNAL) / 1024U));
      Tab5DebugSerial.flush();
      close();
      return false;
    }
    String encoding = _http->header("Transfer-Encoding");
    encoding.toLowerCase();
    _body.reset(encoding.indexOf("chunked") >= 0);
    _codec = tab5RadioCodecHint(_http->header("Content-Type").c_str());
    _stream = _http->getStreamPtr();
    _metaInterval = _http->header("icy-metaint").toInt();
    _bytesUntilMeta = _metaInterval;
    _metadataBytesRemaining = -1;
    _position = 0;
    _opened = _stream != nullptr;
    return _opened;
  }

  uint32_t read(void* data, uint32_t len) override {
    return readInternal(data, len, false);
  }

  uint32_t readNonBlock(void* data, uint32_t len) override {
    return readInternal(data, len, true);
  }

  bool seek(int32_t, int) override { return false; }

  bool close() override {
    _opened = false;
    _stream = nullptr;
    // HTTPClient owns a non-owning client pointer which end() may retain if
    // the peer already disconnected. Destroy HTTP first while transport lives.
    _http.reset();
    delete _client;
    _client = nullptr;
    _url = "";
    _metaInterval = 0;
    _bytesUntilMeta = 0;
    _metadataBytesRemaining = -1;
    _position = 0;
    _body.reset(false);
    _codec = Tab5RadioCodec::Unknown;
    return true;
  }

  bool isOpen() override {
    return _opened && _stream && !_body.finished() && !_body.failed() &&
        (_stream->connected() || _stream->available());
  }

  uint32_t getSize() override { return 0; }
  uint32_t getPos() override { return _position; }
  Tab5RadioCodec codec() const { return _codec; }

 private:
  bool aborted() const { return !tab5NetworkEnabled() || (_abort && _abort->load()); }

  uint32_t readBody(void* data, uint32_t length, bool nonBlock) {
    const uint32_t started = millis();
    while (!aborted() && !_body.finished() && !_body.failed()) {
      const uint32_t got = _body.read(*_stream, data, length);
      if (got) return got;
      if (!_stream->connected() && !_stream->available()) {
        _body.endInput();
        break;
      }
      if (nonBlock || millis() - started >= READ_TIMEOUT_MS) break;
      vTaskDelay(1);
    }
    return 0;
  }

  bool skipIcyMetadata(bool nonBlock) {
    if (_metaInterval <= 0) return true;
    if (_metadataBytesRemaining < 0) {
      uint8_t lengthByte;
      if (readBody(&lengthByte, 1, nonBlock) != 1) return false;
      _metadataBytesRemaining = lengthByte * 16;
    }
    while (_metadataBytesRemaining > 0) {
      uint32_t chunk = static_cast<uint32_t>(_metadataBytesRemaining);
      uint8_t discard[64];
      if (chunk > sizeof(discard)) chunk = sizeof(discard);
      const uint32_t got = readBody(discard, chunk, nonBlock);
      if (got <= 0) return false;
      _metadataBytesRemaining -= static_cast<int32_t>(got);
    }
    _bytesUntilMeta = _metaInterval;
    _metadataBytesRemaining = -1;
    return true;
  }

  uint32_t readInternal(void* data, uint32_t len, bool nonBlock) {
    if (!_opened || !_stream || !data || !len) return 0;
    uint8_t* output = static_cast<uint8_t*>(data);
    uint32_t total = 0;
    while (total < len) {
      if (aborted()) break;
      if (_metaInterval > 0 && _bytesUntilMeta == 0) {
        if (!skipIcyMetadata(nonBlock || total != 0)) break;
      }
      uint32_t chunk = len - total;
      if (_metaInterval > 0 && chunk > static_cast<uint32_t>(_bytesUntilMeta))
        chunk = _bytesUntilMeta;
      const uint32_t got = readBody(output + total, chunk, nonBlock || total != 0);
      if (got <= 0) break;
      total += got;
      _position += got;
      if (_metaInterval > 0) _bytesUntilMeta -= static_cast<int32_t>(got);
    }
    return total;
  }

  static constexpr uint32_t CONNECT_TIMEOUT_MS = 8000;
  static constexpr uint32_t READ_TIMEOUT_MS = 5000;

  std::optional<HTTPClient> _http;
  Tab5RadioBodyReader _body;
  Tab5RadioCodec _codec = Tab5RadioCodec::Unknown;
  NetworkClient* _client = nullptr;
  NetworkClient* _stream = nullptr;
  String _url;
  std::atomic<bool>* _abort = nullptr;
  int32_t _metaInterval = 0;
  int32_t _bytesUntilMeta = 0;
  int32_t _metadataBytesRemaining = -1;
  uint32_t _position = 0;
  bool _opened = false;
};

// One producer and one consumer use the native FreeRTOS stream buffer. Its
// payload is PSRAM; the control block belongs to this object. No ISR uses it.
class Tab5PsramStream {
 public:
  bool create(size_t capacity) {
    destroy();
    // FreeRTOS réserve un octet pour distinguer plein et vide.
    _storage = static_cast<uint8_t*>(
        heap_caps_malloc(capacity + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!_storage) return false;
    _handle = xStreamBufferCreateStatic(capacity + 1, 1, _storage, &_control);
    if (!_handle) {
      heap_caps_free(_storage);
      _storage = nullptr;
      return false;
    }
    _capacity = capacity;
    return true;
  }

  void destroy() {
    if (_handle) vStreamBufferDelete(_handle);
    _handle = nullptr;
    if (_storage) heap_caps_free(_storage);
    _storage = nullptr;
    _capacity = 0;
  }

  size_t capacity() const { return _capacity; }

  size_t used() const {
    return _handle ? xStreamBufferBytesAvailable(_handle) : 0;
  }

  size_t freeSpace() const {
    return _handle ? xStreamBufferSpacesAvailable(_handle) : 0;
  }

  // Producteur uniquement.
  size_t write(const uint8_t* source, size_t length, TickType_t wait = 0) {
    if (!_handle || !source || !length) return 0;
    const size_t sent = xStreamBufferSend(_handle, source, length, 0);
    // Wait for one byte when full, then let the producer publish the newly
    // available span. Waiting for a whole network chunk would delay partial
    // progress until enough room or the cancellation deadline is reached.
    return sent || !wait ? sent : xStreamBufferSend(_handle, source, 1, wait);
  }

  // Consommateur uniquement.
  size_t read(uint8_t* destination, size_t length, TickType_t wait = 0) {
    if (!_handle || !destination || !length) return 0;
    return xStreamBufferReceive(_handle, destination, length, wait);
  }

 private:
  StaticStreamBuffer_t _control = {};
  StreamBufferHandle_t _handle = nullptr;
  uint8_t* _storage = nullptr;
  size_t _capacity = 0;
};

// A receiver feeds compressed bytes independently of the decoder. Closing
// joins that receiver before freeing its transport, buffers or exit semaphore.
class Tab5BufferedRadioSource final : public AudioFileSource {
 public:
  Tab5BufferedRadioSource() = default;
  explicit Tab5BufferedRadioSource(const char* url) { open(url); }
  ~Tab5BufferedRadioSource() override { close(); }

  bool open(const char* url) override {
    const size_t available=heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return open(url,Tab5Resources::radioCapacity(available,available));
  }

  bool open(const char* url, size_t ringBytes) {
    close();
    _abort = false;
    _upstream.setAbortFlag(&_abort);


    while (ringBytes >= 64U * 1024U && !_ring.create(ringBytes)) ringBytes /= 2U;
    if (ringBytes < 64U * 1024U) {
      TAB5_LOG("[radio] ECHEC : file de %u Kio non allouee en PSRAM\n",
               (unsigned)(ringBytes / 1024));
      _upstream.close();
      return false;
    }
    _startBufferBytes = Tab5Resources::radioPreload(ringBytes);
    TAB5_LOG("[radio][MEM] file adaptative=%u Kio, prechargement=%u Kio\n",
             (unsigned)(ringBytes / 1024U), (unsigned)(_startBufferBytes / 1024U));

    _taskExited = xSemaphoreCreateBinary();
    if (!_taskExited) {
      close();
      return false;
    }

    _readBuffer = static_cast<uint8_t*>(heap_caps_malloc(
        NETWORK_READ_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!_readBuffer) { close(); return false; }
    _position = 0;
    _running = true;
    _transportReady = false;
    if (xTaskCreatePinnedToCore(networkTaskEntry, "radio_net",
                                NETWORK_TASK_STACK, this,
                                NETWORK_TASK_PRIORITY, &_task,
                                g_networkTaskCore) != pdPASS) {
      _running = false;
      _task = nullptr;
      close();
      return false;
    }

    // The receive task and its stack exist before the first network burst.
    // HTTP setup still belongs to the audio task, as does the session lease.
    if (!_upstream.open(url)) { close(); return false; }
    tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage::Buffering);
    _opened = true;
    _transportReady = true;
    xTaskNotifyGive(_task);

    // Precharge une fraction de la capacite effectivement obtenue.
    const uint32_t started = millis();
    uint32_t lastLog = 0;
    while (_running && !audioCommandPending() &&
           _ring.used() < _startBufferBytes &&
           millis() - started < START_BUFFER_TIMEOUT_MS) {
      vTaskDelay(pdMS_TO_TICKS(20));
      if (millis() - lastLog >= 1000) {
        lastLog = millis();
        TAB5_LOG("[radio] mise en tampon %u/%u Kio\n",
                 (unsigned)(_ring.used() / 1024),
                 (unsigned)(_startBufferBytes / 1024));
      }
    }
    if (audioCommandPending() || !_ring.used()) {
      close();
      return false;
    }
    // Probe without losing bytes: the decoder receives this prefix first.
    _prefixSize = _ring.read(_prefix, sizeof(_prefix));
    _prefixPosition = 0;
    TAB5_LOG("[radio] demarrage avec %u Kio d'avance (~%u s)\n",
             (unsigned)(_ring.used() / 1024),
             (unsigned)(_ring.used() / 16000));
    return true;
  }

  uint32_t read(void* data, uint32_t len) override {
    if (!data || !len) return 0;
    const uint32_t prefix = _prefixPosition < _prefixSize ? readPrefix(data, len) : 0;
    if (prefix == len) return prefix;

    // ESP8266Audio accepts a short read. Only an empty ring may wait; its
    // producer wakes the native stream-buffer receiver as soon as data arrives.
    // The bounded wait also observes commands without an extra sleeping phase.
    size_t got = _ring.read(static_cast<uint8_t*>(data) + prefix, len - prefix);
    if (prefix) {
      _position += static_cast<uint32_t>(got);
      return prefix + static_cast<uint32_t>(got);
    }
    if (!got) {
      const uint32_t started = millis();
      while (_running && !_abort && !audioCommandPending() && !got &&
             millis() - started < READ_TOTAL_TIMEOUT_MS) {
        got = _ring.read(static_cast<uint8_t*>(data), len,
                         pdMS_TO_TICKS(CONSUMER_WAIT_MS));
      }
      if (!got) {
        ++_refillWaits;
        return 0;
      }
    }
    if (got < len) ++_shortReads;
    _position += static_cast<uint32_t>(got);
    return static_cast<uint32_t>(got);
  }

  uint32_t readNonBlock(void* data, uint32_t len) override {
    if (!data || !len) return 0;
    const uint32_t prefix = _prefixPosition < _prefixSize ? readPrefix(data, len) : 0;
    const size_t got = _ring.read(static_cast<uint8_t*>(data) + prefix, len - prefix);
    _position += static_cast<uint32_t>(got);
    return prefix + static_cast<uint32_t>(got);
  }

  bool seek(int32_t, int) override { return false; }

  bool close() override {
    _opened = false;
    _abort = true;
    _running = false;
    if (_task && _taskExited) {
      // The receiver also exists while HTTP setup is pending. Publish its
      // one-shot start notification on setup failure so it can exit and join.
      // After that notification it may already have exited: never notify that
      // handle again from close().
      if (!_transportReady.exchange(true)) xTaskNotifyGive(_task);
      // La tache peut etre bloquee dans une lecture TLS jusqu'a READ_TIMEOUT_MS.
      // Il est interdit de detruire `_upstream`, le semaphore ou la file tant
      // qu'elle n'a pas confirme sa sortie : elle conserve un pointeur vers
      // cet objet. L'ancienne limite de 4 s expirait avant le timeout reseau
      // de 5 s et transformait une simple coupure serveur en use-after-free.
      if (xSemaphoreTake(_taskExited,
                         pdMS_TO_TICKS(TASK_EXIT_WARNING_MS)) != pdTRUE) {
        TAB5_LOG("[radio] fermeture reseau > %lu ms, attente securisee\n",
                 (unsigned long)TASK_EXIT_WARNING_MS);
        xSemaphoreTake(_taskExited, portMAX_DELAY);
      }
    }
    _task = nullptr;
    if (_taskExited) {
      vSemaphoreDelete(_taskExited);
      _taskExited = nullptr;
    }
    _upstream.close();
    _ring.destroy();
    heap_caps_free(_readBuffer);
    _readBuffer = nullptr;
    _position = 0;
    _prefixPosition = _prefixSize = 0;
    return true;
  }

  bool isOpen() override {
    return _opened && (_running || _ring.used() > 0 || _prefixPosition < _prefixSize);
  }

  uint32_t getSize() override { return 0; }
  uint32_t getPos() override { return _position; }
  bool loop() override { return isOpen(); }

  size_t bufferedBytes() const { return _ring.used(); }
  size_t capacityBytes() const { return _ring.capacity(); }
  // Reads that remained empty until cancellation, receiver exit or timeout.
  uint32_t refillWaits() const { return _refillWaits; }
  uint32_t shortReads() const { return _shortReads; }
  uint32_t receivedBytes() const { return _receivedBytes.load(); }
  bool receiving() const { return _running.load(); }
  uint32_t networkStackFreeBytes() const { return _networkStackFreeBytes; }
  Tab5RadioCodec codec(Tab5RadioCodec hint, const char* url) const {
    return tab5RadioSelectCodec(_prefix, _prefixSize, _upstream.codec(), hint, url);
  }

 private:
  uint32_t readPrefix(void* data, uint32_t len) {
    const size_t available = _prefixSize - _prefixPosition;
    const size_t count = len < available ? len : available;
    memcpy(data, _prefix + _prefixPosition, count);
    _prefixPosition += count;
    _position += static_cast<uint32_t>(count);
    return static_cast<uint32_t>(count);
  }
  static void networkTaskEntry(void* argument) {
    static_cast<Tab5BufferedRadioSource*>(argument)->networkTask();
  }

  void networkTask() {
    // No socket work exists until the owner publishes setup or abort. The
    // notification is sent once, before a possible join, and is consumed here
    // before this task uses stream-buffer notifications for data backpressure.
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint32_t yieldedAt = millis();
    while (_running && !_abort && _upstream.isOpen()) {
      _networkStackFreeBytes = uxTaskGetStackHighWaterMark(nullptr);
      const uint32_t got = _upstream.read(_readBuffer, NETWORK_READ_SIZE);
      if (!got) {
        if (_abort || !_upstream.isOpen()) break;
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }

      size_t sent = 0;
      while (_running && !_abort && sent < got) {
        const size_t written = _ring.write(_readBuffer + sent, got - sent,
                                           pdMS_TO_TICKS(20));
        if (!written) continue;
        sent += written;
      }
      _receivedBytes += got;
      // Drain bursts cooperatively, without inserting one scheduler tick for
      // every 2 KiB. The stream buffer and TCP still apply backpressure.
      if (millis() - yieldedAt >= 4U) {
        vTaskDelay(1);
        yieldedAt = millis();
      }
    }
    _running = false;
    _networkStackFreeBytes = uxTaskGetStackHighWaterMark(nullptr);
    SemaphoreHandle_t exited = _taskExited;
    if (exited) xSemaphoreGive(exited);
    vTaskDelete(nullptr);
  }

  // Capacity is chosen at open from the currently available PSRAM, with a
  // smaller retry on allocation failure. The initial preload is a fraction
  // of that actual capacity. Streaming servers differ in burst behavior;
  // neither a fixed amount of buffered time nor automatic refill is promised.
  size_t _startBufferBytes = 0;
  static constexpr size_t NETWORK_READ_SIZE = 16U * 1024U;
  uint8_t* _readBuffer = nullptr;
  std::atomic<uint32_t> _receivedBytes{0};
  // Sans rafale de la part du serveur, 96 Kio demandent 6 s reelles.
  static constexpr uint32_t START_BUFFER_TIMEOUT_MS = 15000;
  static constexpr uint32_t CONSUMER_WAIT_MS = 5;
  static constexpr uint32_t READ_TOTAL_TIMEOUT_MS = 8000;
  // Superieur au timeout de lecture HTTPS. Apres cet avertissement, close()
  // continue d'attendre : liberer la source serait une corruption memoire.
  static constexpr uint32_t TASK_EXIT_WARNING_MS = 6000;
  static constexpr uint32_t NETWORK_TASK_STACK = 16 * 1024;

  Tab5HttpAudioSource _upstream;
  uint8_t _prefix[16] = {};
  size_t _prefixPosition = 0, _prefixSize = 0;
  Tab5PsramStream _ring;
  SemaphoreHandle_t _taskExited = nullptr;
  TaskHandle_t _task = nullptr;
  std::atomic<bool> _running{false};
  std::atomic<bool> _transportReady{false};
  std::atomic<bool> _abort{false};
  uint32_t _refillWaits = 0;
  uint32_t _shortReads = 0;
  std::atomic<uint32_t> _networkStackFreeBytes{0};
  uint32_t _position = 0;
  bool _opened = false;
};

Tab5AudioEngine Tab5Audio;

class Tab5AudioEngine::StateGuard {
 public:
  explicit StateGuard(const Tab5AudioEngine& engine,
                      TickType_t timeout = portMAX_DELAY)
      : _engine(engine), _locked(engine.lockState(timeout)) {}
  ~StateGuard() {
    if (_locked) _engine.unlockState();
  }
  explicit operator bool() const { return _locked; }

 private:
  const Tab5AudioEngine& _engine;
  bool _locked = false;
};

static bool isAudioPath(const String& path) {
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".mp3") || lower.endsWith(".aac") ||
         lower.endsWith(".wav") || lower.endsWith(".flac");
}

static bool isRecordingPath(const String& path) {
  return path == RECORD_ROOT || path.startsWith(String(RECORD_ROOT) + "/");
}

static uint32_t readBigEndian32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) |
         static_cast<uint32_t>(data[3]);
}

static uint32_t readLittleEndian32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

static uint32_t estimateMp3Duration(const String& path) {
  File file = tab5FS.open(path.c_str(), FILE_READ);
  if (!file) return 0;

  const uint32_t fileSize = static_cast<uint32_t>(file.size());
  uint32_t audioStart = 0;
  uint8_t id3[10] = {};
  if (file.read(id3, sizeof(id3)) == sizeof(id3) &&
      id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
    const uint32_t tagSize =
        (static_cast<uint32_t>(id3[6] & 0x7f) << 21) |
        (static_cast<uint32_t>(id3[7] & 0x7f) << 14) |
        (static_cast<uint32_t>(id3[8] & 0x7f) << 7) |
        static_cast<uint32_t>(id3[9] & 0x7f);
    audioStart = 10U + tagSize + ((id3[5] & 0x10U) ? 10U : 0U);
  }
  if (audioStart >= fileSize || !file.seek(audioStart)) {
    file.close();
    return 0;
  }

  uint8_t probe[1024] = {};
  const size_t got = file.read(probe, sizeof(probe));
  file.close();
  if (got < 4) return 0;

  static constexpr uint16_t bitrateMpeg1[16] = {
      0, 32, 40, 48, 56, 64, 80, 96,
      112, 128, 160, 192, 224, 256, 320, 0};
  static constexpr uint16_t bitrateMpeg2[16] = {
      0, 8, 16, 24, 32, 40, 48, 56,
      64, 80, 96, 112, 128, 144, 160, 0};
  static constexpr uint32_t sampleRates[3] = {44100, 48000, 32000};

  for (size_t i = 0; i + 4 <= got; ++i) {
    if (probe[i] != 0xff || (probe[i + 1] & 0xe0U) != 0xe0U) continue;
    const uint8_t version = (probe[i + 1] >> 3) & 0x03U;
    const uint8_t layer = (probe[i + 1] >> 1) & 0x03U;
    const uint8_t bitrateIndex = (probe[i + 2] >> 4) & 0x0fU;
    const uint8_t sampleIndex = (probe[i + 2] >> 2) & 0x03U;
    if (version == 1 || layer != 1 || bitrateIndex == 0 ||
        bitrateIndex == 15 || sampleIndex == 3) continue;

    const bool mpeg1 = version == 3;
    const uint32_t bitrateKbps = mpeg1
        ? bitrateMpeg1[bitrateIndex] : bitrateMpeg2[bitrateIndex];
    uint32_t sampleRate = sampleRates[sampleIndex];
    if (version == 2) sampleRate /= 2;
    else if (version == 0) sampleRate /= 4;
    if (!bitrateKbps || !sampleRate) continue;

    const bool mono = ((probe[i + 3] >> 6) & 0x03U) == 3;
    const size_t crcBytes = (probe[i + 1] & 0x01U) ? 0U : 2U;
    const size_t sideInfo = mpeg1 ? (mono ? 17U : 32U)
                                  : (mono ? 9U : 17U);
    const size_t xing = i + 4U + crcBytes + sideInfo;
    if (xing + 12U <= got &&
        ((!memcmp(probe + xing, "Xing", 4)) ||
         (!memcmp(probe + xing, "Info", 4)))) {
      const uint32_t flags = readBigEndian32(probe + xing + 4);
      if (flags & 0x01U) {
        const uint32_t frames = readBigEndian32(probe + xing + 8);
        const uint32_t samplesPerFrame = mpeg1 ? 1152U : 576U;
        if (frames) {
          return static_cast<uint32_t>(
              (static_cast<uint64_t>(frames) * samplesPerFrame +
               sampleRate / 2U) / sampleRate);
        }
      }
    }

    const size_t vbri = i + 4U + 32U;
    if (vbri + 18U <= got && !memcmp(probe + vbri, "VBRI", 4)) {
      const uint32_t frames = readBigEndian32(probe + vbri + 14);
      const uint32_t samplesPerFrame = mpeg1 ? 1152U : 576U;
      if (frames) {
        return static_cast<uint32_t>(
            (static_cast<uint64_t>(frames) * samplesPerFrame +
             sampleRate / 2U) / sampleRate);
      }
    }

    const uint32_t frameStart = audioStart + static_cast<uint32_t>(i);
    const uint32_t audioBytes = fileSize > frameStart ? fileSize - frameStart : 0;
    if (audioBytes) {
      return static_cast<uint32_t>(
          (static_cast<uint64_t>(audioBytes) * 8ULL +
           bitrateKbps * 500ULL) / (bitrateKbps * 1000ULL));
    }
  }
  return 0;
}

static uint32_t estimateWavDuration(const String& path) {
  File file = tab5FS.open(path.c_str(), FILE_READ);
  if (!file) return 0;
  uint8_t header[44] = {};
  const size_t got = file.read(header, sizeof(header));
  file.close();
  if (got < sizeof(header) || memcmp(header, "RIFF", 4) ||
      memcmp(header + 8, "WAVE", 4)) return 0;
  const uint32_t byteRate = readLittleEndian32(header + 28);
  const uint32_t dataBytes = readLittleEndian32(header + 40);
  return byteRate ? (dataBytes + byteRate / 2U) / byteRate : 0;
}

static uint32_t estimateFlacDuration(const String& path) {
  File file = tab5FS.open(path.c_str(), FILE_READ);
  if (!file) return 0;
  uint8_t header[42] = {};
  const size_t got = file.read(header, sizeof(header));
  file.close();
  if (got < sizeof(header) || memcmp(header, "fLaC", 4) ||
      (header[4] & 0x7fU) != 0 ||
      (((static_cast<uint32_t>(header[5]) << 16) |
        (static_cast<uint32_t>(header[6]) << 8) | header[7]) < 34U)) {
    return 0;
  }

  // STREAMINFO compacte le debit (20 bits), les canaux (3), la profondeur
  // (5) et le nombre total d'echantillons (36) dans les octets 10..17.
  const uint8_t* packed = header + 18;
  const uint32_t sampleRate =
      (static_cast<uint32_t>(packed[0]) << 12) |
      (static_cast<uint32_t>(packed[1]) << 4) |
      (static_cast<uint32_t>(packed[2]) >> 4);
  const uint64_t totalSamples =
      (static_cast<uint64_t>(packed[3] & 0x0fU) << 32) |
      (static_cast<uint64_t>(packed[4]) << 24) |
      (static_cast<uint64_t>(packed[5]) << 16) |
      (static_cast<uint64_t>(packed[6]) << 8) |
      static_cast<uint64_t>(packed[7]);
  if (!sampleRate || !totalSamples) return 0;
  return static_cast<uint32_t>((totalSamples + sampleRate / 2U) / sampleRate);
}

static uint32_t estimateLocalDuration(const String& path) {
  String lower = path;
  lower.toLowerCase();
  if (lower.endsWith(".mp3")) return estimateMp3Duration(path);
  if (lower.endsWith(".wav")) return estimateWavDuration(path);
  if (lower.endsWith(".flac")) return estimateFlacDuration(path);
  return 0;
}

static String baseName(const String& path) {
  const int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

static String parentFolder(const String& path) {
  const int slash = path.lastIndexOf('/');
  if (slash <= 0) return "/";
  return path.substring(0, slash);
}

static bool writeLe16(File& file, uint16_t value) {
  uint8_t bytes[2] = {
      static_cast<uint8_t>(value & 0xFF),
      static_cast<uint8_t>((value >> 8) & 0xFF),
  };
  return file.write(bytes, sizeof(bytes)) == sizeof(bytes);
}

static bool writeLe32(File& file, uint32_t value) {
  uint8_t bytes[4] = {
      static_cast<uint8_t>(value & 0xFF),
      static_cast<uint8_t>((value >> 8) & 0xFF),
      static_cast<uint8_t>((value >> 16) & 0xFF),
      static_cast<uint8_t>((value >> 24) & 0xFF),
  };
  return file.write(bytes, sizeof(bytes)) == sizeof(bytes);
}

static bool flushRecordingFile(File& file) {
  // Arduino FS flush() discards fflush/fsync return values; writeError alone
  // does not reflect these failures. Capture errno before any later FS call.
  errno = 0;
  file.flush();
  const int flushError = errno;
  const bool writeError = file.getWriteError() != 0;
  errno = flushError ? flushError : (writeError ? EIO : 0);
  return errno == 0;
}

bool Tab5AudioEngine::lockState(TickType_t timeout) const {
  return !_stateMutex ||
         xSemaphoreTakeRecursive(_stateMutex, timeout) == pdTRUE;
}

void Tab5AudioEngine::unlockState() const {
  if (_stateMutex) xSemaphoreGiveRecursive(_stateMutex);
}

// Scalar callers must not copy the six text fields of a full snapshot.
// Keep the same synchronization and failure defaults as snapshot().
template <typename T>
static T audioSnapshotValue(SemaphoreHandle_t mutex,
                            const Tab5AudioSnapshot& snapshot,
                            T Tab5AudioSnapshot::*member) {
  if (!mutex || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return T{};
  T result = snapshot.*member;
  xSemaphoreGive(mutex);
  return result;
}

bool Tab5AudioEngine::snapshot(Tab5AudioSnapshot& result) const {
  if (!_snapshotMutex) return false;
  if (xSemaphoreTake(_snapshotMutex, portMAX_DELAY) != pdTRUE) return false;
  result = _snapshot;
  xSemaphoreGive(_snapshotMutex);
  return true;
}

bool Tab5AudioEngine::indicatorSnapshot(Tab5AudioSnapshot& result,
                                         uint32_t& seen, bool force) const {
  if (!force && seen == _indicatorRevision.load(std::memory_order_acquire))
    return false;
  if (!_snapshotMutex || xSemaphoreTake(_snapshotMutex, 0) != pdTRUE)
    return false;
  const uint32_t revision = _indicatorRevision.load(std::memory_order_relaxed);
  const bool changed = force || seen != revision;
  if (changed) {
    result = _snapshot;
    // Arduino String assignment can fail without an exception. Keep the event
    // pending so a low-memory copy never becomes the last displayed title/path.
    if (result.nowPlaying != _snapshot.nowPlaying ||
        result.currentPath != _snapshot.currentPath ||
        result.currentStationUrl != _snapshot.currentStationUrl) {
      xSemaphoreGive(_snapshotMutex);
      return false;
    }
    seen = revision;
  }
  xSemaphoreGive(_snapshotMutex);
  return changed;
}

bool Tab5AudioEngine::mediaSnapshot(Tab5AudioSnapshot& result,
                                    uint32_t& seen, bool force) const {
  if (!force && seen == _mediaRevision.load(std::memory_order_acquire))
    return false;
  if (!_snapshotMutex || xSemaphoreTake(_snapshotMutex, 0) != pdTRUE)
    return false;
  const uint32_t revision = _mediaRevision.load(std::memory_order_relaxed);
  const bool changed = force || seen != revision;
  if (changed) {
    result = _snapshot;
    if (result.nowPlaying != _snapshot.nowPlaying ||
        result.status != _snapshot.status ||
        result.lastRecordingPath != _snapshot.lastRecordingPath ||
        result.currentPath != _snapshot.currentPath ||
        result.currentStationUrl != _snapshot.currentStationUrl ||
        result.savedWifiSsid != _snapshot.savedWifiSsid) {
      xSemaphoreGive(_snapshotMutex);
      return false;
    }
    seen = revision;
  }
  xSemaphoreGive(_snapshotMutex);
  return changed;
}

bool Tab5AudioEngine::wifiSnapshot(bool& enabled, bool& connected) const {
  if (!_snapshotMutex || xSemaphoreTake(_snapshotMutex, 0) != pdTRUE)
    return false;
  enabled = _snapshot.wifiEnabled;
  connected = _snapshot.wifiConnected;
  xSemaphoreGive(_snapshotMutex);
  return true;
}

bool Tab5AudioEngine::routingSnapshot(Tab5AudioRoutingSnapshot& result) const {
  if (!_snapshotMutex) return false;
  if (xSemaphoreTake(_snapshotMutex, portMAX_DELAY) != pdTRUE) return false;
  result = {_snapshot.mode, _snapshot.paused, _snapshot.playing,
            _snapshot.wifiConnected, _snapshot.radioBufferedBytes,
            _snapshot.radioBufferCapacity};
  xSemaphoreGive(_snapshotMutex);
  return true;
}

void Tab5AudioEngine::serviceWifiSignal() {
  if (!_audioTask || xTaskGetCurrentTaskHandle() != _audioTask) return;
  if (!_wifiEnabled || WiFi.status() != WL_CONNECTED) {
    _wifiRssiCached = 0;
    _wifiSignalSampled = false;
    return;
  }
  // RSSI is a P4-to-C6 request. Keep it out of UI/state locks and out of
  // continuous playback. Sample on connection or an explicit UI request.
  if (_mode != Tab5AudioMode::Idle || _wifiPending || _wifiScanRunning) return;
  const uint32_t now = millis();
  if (_wifiSignalSampled && (!_wifiSignalRequested.load(std::memory_order_acquire) ||
      now - _lastWifiSignalAt < 5000U)) return;
  // Consume before the RPC so a new UI request during it remains pending.
  _wifiSignalRequested.exchange(false, std::memory_order_acq_rel);
  _lastWifiSignalAt = now;
  _wifiSignalSampled = true;
  const int32_t signal = WiFi.RSSI();
  if (signal < 0) _wifiRssiCached = signal;
}

void Tab5AudioEngine::publishSnapshot() {
  if (!_snapshotMutex) return;
  const uint32_t now = millis();
  if (!_uiDirty && now - _lastSnapshotAt < 50) return;
  _lastSnapshotAt = now;

  const bool recordingNow = _mode == Tab5AudioMode::Recording;
  const bool wifiConnectedNow = WiFi.status() == WL_CONNECTED;
  uint16_t progress = 0;
  if (_mode == Tab5AudioMode::Local && _localSource) {
    const uint32_t size = _localSource->getSize();
    if (size) {
      const uint64_t scaled =
          static_cast<uint64_t>(_localSource->getPos()) * 1000ULL;
      const uint32_t value = static_cast<uint32_t>(scaled / size);
      progress = static_cast<uint16_t>(value > 1000U ? 1000U : value);
    }
  }
  uint32_t elapsed = 0;
  if ((_mode == Tab5AudioMode::Local || _mode == Tab5AudioMode::Radio) &&
      _playStartedAt) {
    const uint32_t clockNow = _paused && _pauseStartedAt
        ? _pauseStartedAt : now;
    elapsed = (clockNow - _playStartedAt) / 1000U;
  }
  if (_mode == Tab5AudioMode::Local && !_localEstimatedSeconds &&
      progress >= 50 && elapsed >= 5) {
    _localEstimatedSeconds = static_cast<uint32_t>(
        (static_cast<uint64_t>(elapsed) * 1000ULL) / progress);
  }
  const uint32_t estimated = _mode == Tab5AudioMode::Local
      ? _localEstimatedSeconds : 0;
  const bool playingNow = !_paused &&
      (decoderRunning() || (_mode == Tab5AudioMode::Radio && _wifiPending));
  const uint32_t recordingSecondsNow = recordingNow
      ? _recordDataBytes / (RECORD_SAMPLE_RATE * sizeof(int16_t)) : 0;

  if (xSemaphoreTake(_snapshotMutex, portMAX_DELAY) == pdTRUE) {
    const String* stationUrl = _mode == Tab5AudioMode::Radio
        ? &_selectedRadioUrl : nullptr;
    const bool indicatorChanged = _snapshot.mode != _mode ||
        _snapshot.paused != _paused || _snapshot.volume != _volume ||
        _snapshot.nowPlaying != _nowPlaying || _snapshot.currentPath != _currentLocalPath ||
        (stationUrl ? _snapshot.currentStationUrl != *stationUrl
                    : _snapshot.currentStationUrl.length() != 0);
    const bool mediaChanged = indicatorChanged ||
        _snapshot.playing != playingNow ||
        _snapshot.recording != recordingNow || _snapshot.status != _status ||
        _snapshot.playbackProgressPermille != progress ||
        _snapshot.playbackElapsedSeconds != elapsed ||
        _snapshot.playbackEstimatedSeconds != estimated ||
        _snapshot.recordingSeconds != recordingSecondsNow ||
        (recordingNow && _snapshot.recordingLevel != _recordLevel);
    _snapshot.mode = _mode;
    _snapshot.paused = _paused;
    _snapshot.recording = recordingNow;
    _snapshot.playing = playingNow;
    _snapshot.wifiEnabled = _wifiEnabled;
    _snapshot.wifiConnected = wifiConnectedNow;
    _snapshot.wifiConnecting = _wifiPending;
    _snapshot.wifiAutoStart = _wifiAutoStart;
    _snapshot.hasSavedWifi = _wifiSsid.length() > 0;
    _snapshot.wifiRssi = wifiConnectedNow ? _wifiRssiCached : 0;
    _snapshot.volume = _volume;
    _snapshot.recordingLevel = _recordLevel;
    _snapshot.recordingSeconds = recordingSecondsNow;
    _snapshot.playbackProgressPermille = progress;
    _snapshot.playbackElapsedSeconds = elapsed;
    _snapshot.playbackEstimatedSeconds = estimated;
    _snapshot.radioBufferedBytes = _radioSource
        ? static_cast<uint32_t>(_radioSource->bufferedBytes()) : 0;
    _snapshot.radioBufferCapacity = _radioSource
        ? static_cast<uint32_t>(_radioSource->capacityBytes()) : 0;
    if (_snapshot.nowPlaying != _nowPlaying) _snapshot.nowPlaying = _nowPlaying;
    if (_snapshot.status != _status) _snapshot.status = _status;
    if (_snapshot.lastRecordingPath != _lastRecordingPath)
      _snapshot.lastRecordingPath = _lastRecordingPath;
    if (_snapshot.currentPath != _currentLocalPath)
      _snapshot.currentPath = _currentLocalPath;
    if (stationUrl) {
      if (_snapshot.currentStationUrl != *stationUrl) _snapshot.currentStationUrl = *stationUrl;
    } else if (_snapshot.currentStationUrl.length()) _snapshot.currentStationUrl = "";
    if (_snapshot.savedWifiSsid != _wifiSsid)
      _snapshot.savedWifiSsid = _wifiSsid;
    if (_uiDirty) _snapshotDirty = true;
    if (indicatorChanged) {
      uint32_t revision = _indicatorRevision.load(std::memory_order_relaxed) + 1;
      if (!revision) revision = 1;
      _indicatorRevision.store(revision, std::memory_order_release);
    }
    if (mediaChanged) {
      uint32_t revision = _mediaRevision.load(std::memory_order_relaxed) + 1;
      if (!revision) revision = 1;
      _mediaRevision.store(revision, std::memory_order_release);
    }
    xSemaphoreGive(_snapshotMutex);
  }
  _uiDirty = false;
}

void Tab5AudioEngine::audioTaskEntry(void* argument) {
  auto* engine = static_cast<Tab5AudioEngine*>(argument);
  // The RTOS can schedule the new task before xTaskCreate returns. Do not
  // inspect engine state until the UI has published its handle and ownership.
  while (engine->_workerLifecycle.starting()) vTaskDelay(1);
  for (;;) {
    bool realtime = false, retire = false;
    {
      Tab5StorageUse storage{true};
      if (storage) {
        engine->serviceMemoryPriority();
        engine->drainCommands();
        engine->serviceDecoder();
        engine->serviceWifiSignal();
        {
          StateGuard guard(*engine, pdMS_TO_TICKS(10));
          if (guard) engine->publishSnapshot();
        }
        realtime = (!engine->_paused && engine->decoderRunning()) ||
            engine->_mode == Tab5AudioMode::Recording;
        retire = engine->canRetireAudioTask();
      }
    }
    if (retire) {
      tab5TaskCheckpoint("audio idle");
      // All storage/state scopes have ended and no decoder, socket or recording
      // belongs to this worker. A late queued command remains in the queue and
      // the next UI service starts its successor. The lifecycle store is the
      // final engine access; returning lets the task trampoline self-delete.
      engine->_audioTask.store(nullptr, std::memory_order_release);
      engine->_workerLifecycle.stopped();
      return;
    }
    if (realtime) vTaskDelay(1);
    else {
      // A queued command wakes the owner immediately. The timeout services
      // Wi-Fi transitions and memory-priority requests that have no command.
      Tab5AudioCommand pending;
      xQueuePeek(engine->_commandQueue, &pending, pdMS_TO_TICKS(20));
    }
  }
}

bool Tab5AudioEngine::canRetireAudioTask() const {
  StateGuard guard(*this, 0);
  if (!guard) return false;
  return !_localSource && !_streamSource && !_radioSource && !_radioLease &&
      !_mp3 && !_wav && !_aac && !_flac && !_recordFile &&
      _mode != Tab5AudioMode::Recording && !_wifiPending && !_wifiScanRunning &&
      !_wifiScanQueued && !_playRadioAfterWifi && !_radioReconnectPending &&
      !_priorityResume && !_priorityCommandPending &&
      _memoryPriority.state() == Tab5AudioPriorityControl::Idle &&
      (!_commandQueue || uxQueueMessagesWaiting(_commandQueue) == 0);
}

size_t Tab5AudioEngine::reclaimIdleWorker() {
  // The OS only requests collection. It never deletes another task or closes
  // the audio owner's resources. Active playback/recordings continue normally.
  _idleReleaseRequested.store(true, std::memory_order_release);
  return 0; // FreeRTOS idle cleanup, not this callback, returns the stack bytes.
}

uint32_t Tab5AudioEngine::acquireMemoryPriority(uint32_t timeoutMs) {
  // This waiting API belongs to a background job. The UI and audio owner
  // must stay runnable to acknowledge the request and publish its status.
  if (xTaskGetCurrentTaskHandle() == _audioTask.load(std::memory_order_acquire)) return 0;
  const uint32_t token = _memoryPriority.request();
  if (!token) return 0;
  const uint32_t started = millis();
  do {
    const auto state = _memoryPriority.state(token);
    if (state == Tab5AudioPriorityControl::Held) return token;
    if (state == Tab5AudioPriorityControl::Denied ||
        state == Tab5AudioPriorityControl::Idle) break;
    vTaskDelay(pdMS_TO_TICKS(10));
  } while (millis() - started < timeoutMs);
  // A slow close may still be in progress. Release marks that generation;
  // the audio worker restores it itself after its last reader has exited.
  _memoryPriority.release(token);
  return 0;
}

void Tab5AudioEngine::releaseMemoryPriority(uint32_t token) {
  _memoryPriority.release(token);
}

bool Tab5AudioEngine::deferPriorityCommand(const Tab5AudioCommand& command) {
  const auto state = _memoryPriority.state();
  if (state != Tab5AudioPriorityControl::Held &&
      state != Tab5AudioPriorityControl::Release) return false;
  switch (command.type) {
    case Tab5AudioCommandType::Stop:
      _priorityResume = false;
      _priorityCommandPending = false;
      return false;  // Stop is immediate and cancels the automatic resume.
    case Tab5AudioCommandType::PlayTrack:
    case Tab5AudioCommandType::PlayPath:
    case Tab5AudioCommandType::PlayStation:
    case Tab5AudioCommandType::Next:
    case Tab5AudioCommandType::Previous:
    case Tab5AudioCommandType::StartRecording:
      // Latest user intent wins; do not restart a decoder inside the print's
      // reserved interval. Stop is processed above and never deferred.
      _priorityCommand = command;
      _priorityCommandPending = true;
      _priorityResume = false;
      setStatus("Lecture prévue après l’activité prioritaire");
      return true;
    case Tab5AudioCommandType::TogglePause:
      if (_mode == Tab5AudioMode::Local || _mode == Tab5AudioMode::Radio) {
        _priorityWasPaused = !_priorityWasPaused;
        _priorityResume = true;
        _priorityCommandPending = false;
        setStatus(_priorityWasPaused ? "Pause" : "Lecture prévue après l’activité prioritaire");
      }
      return true;
    case Tab5AudioCommandType::SeekPermille:
      _priorityPosition = command.index > 1000 ? 1000 : command.index;
      return true;
    case Tab5AudioCommandType::WifiEnable:
      if (!command.flag && _priorityWasRadio) _priorityResume = false;
      return false;
    case Tab5AudioCommandType::WifiForget:
    case Tab5AudioCommandType::WifiConnect:
      if (_priorityWasRadio) _priorityResume = false;
      return false;
    default: return false;
  }
}

void Tab5AudioEngine::serviceMemoryPriority() {
  const uint32_t requested = _memoryPriority.begin();
  if (requested) {
    if (_mode == Tab5AudioMode::Recording) {
      _memoryPriority.acknowledge(requested, false);
      return;
    }
    _priorityWasRadio = _mode == Tab5AudioMode::Radio;
    _priorityWasPaused = _paused;
    _priorityPath = _currentLocalPath;
    _priorityPosition = playbackProgressPermille();
    _priorityResume = decoderRunning() || (_priorityWasRadio && !_paused &&
        (_wifiPending || _radioReconnectPending || _playRadioAfterWifi));
    _priorityCommandPending = false;
    if (_priorityResume || Tab5AudioDevice.owner() == Tab5AudioClient::Audio) {
      // A paused radio keeps only its quiescent output. It must still hand
      // that output back before acknowledging another application's priority.
      cleanupDecoder();  // own task: joins receiver, closes TLS, then I2S
      _paused = true;
      _playRadioAfterWifi = false;
      _radioReconnectPending = false;
      setStatus("Audio suspendu pour une activité prioritaire");
      tab5LogMemory("audio libere pour activite prioritaire");
    }
    _memoryPriority.acknowledge(requested, true);
  }
  const uint32_t released = _memoryPriority.released();
  if (!released) return;
  // Consume user intent already queued before resuming the saved stream.
  // During Release, handlers defer new playback and execute Stop immediately.
  const UBaseType_t pending = _commandQueue
      ? uxQueueMessagesWaiting(_commandQueue) : 0;
  for (UBaseType_t handled = 0; handled < pending; handled += 4) drainCommands();
  const bool resume = _priorityResume;
  const bool commandPending = _priorityCommandPending;
  const Tab5AudioCommand command = _priorityCommand;
  _priorityResume = false;
  _priorityCommandPending = false;
  // Once idle, a deferred command must pass the ordinary handler. No caller
  // may mutate audio resources; a later request waits for this worker again.
  _memoryPriority.complete(released);
  if (commandPending) {
    handleCommand(command);
  } else if (resume) {
    if (_priorityWasRadio) {
      if (_priorityWasPaused) { _paused = true; setStatus("Pause"); }
      else resumeSelectedStation();
    } else if (_priorityPath.length() && startLocalDecoder(_priorityPath)) {
      if (_priorityPosition) execSeekPermille(_priorityPosition);
      if (_priorityWasPaused) execTogglePause();
    }
  }
  _priorityPath = "";
}

bool Tab5AudioEngine::postCommand(const Tab5AudioCommand& command) {
  if (!_commandQueue) return false;
  if (command.type == Tab5AudioCommandType::Stop) {
    // Stop cancels queued play/seek commands and must not be lost to a full queue.
    StateGuard guard(*this);
    if (!guard) return false;
    xQueueReset(_commandQueue);
    if (_wifiScanQueued) {
      _wifiScanQueued = false;
      setWifiScanStatus("Arrêté", true);
    }
    return xQueueSendToFront(_commandQueue, &command, 0) == pdTRUE;
  }
  // Envoi sans attente : l'interface ne doit jamais se bloquer sur le moteur.
  return xQueueSend(_commandQueue, &command, 0) == pdTRUE;
}

void Tab5AudioEngine::drainCommands() {
  if (!_commandQueue) return;
  Tab5AudioCommand command;
  // Borne le nombre de commandes traitées par tour afin qu'une rafale
  // d'appuis ne retarde pas le décodage au-delà d'un cycle.
  for (uint8_t handled = 0; handled < 4; ++handled) {
    if (xQueueReceive(_commandQueue, &command, 0) != pdTRUE) return;
    handleCommand(command);
  }
}

void Tab5AudioEngine::handleCommand(const Tab5AudioCommand& command) {
  if (deferPriorityCommand(command)) return;
  // Cette trace permet de distinguer sans ambiguïté une fermeture du serveur
  // d'une commande envoyée par l'interface. Aucun mot de passe n'est affiché.
  switch (command.type) {
    case Tab5AudioCommandType::PlayTrack:
      TAB5_LOG("[audio-cmd] PlayTrack index=%lu\n",
               (unsigned long)command.index);
      break;
    case Tab5AudioCommandType::PlayPath:
      TAB5_LOG("[audio-cmd] PlayPath chemin=%s\n", command.text);
      break;
    case Tab5AudioCommandType::SeekPermille:
      TAB5_LOG("[audio-cmd] Seek position=%lu/1000\n",
               (unsigned long)command.index);
      break;
    case Tab5AudioCommandType::PlayStation:
      TAB5_LOG("[audio-cmd] PlayStation index=%lu\n",
               (unsigned long)command.index);
      break;
    case Tab5AudioCommandType::Next:
      TAB5_LOG("[audio-cmd] Next\n");
      break;
    case Tab5AudioCommandType::Previous:
      TAB5_LOG("[audio-cmd] Previous\n");
      break;
    case Tab5AudioCommandType::TogglePause:
      TAB5_LOG("[audio-cmd] TogglePause\n");
      break;
    case Tab5AudioCommandType::Stop:
      TAB5_LOG("[audio-cmd] Stop\n");
      break;
    default:
      break;
  }
  switch (command.type) {
    case Tab5AudioCommandType::PlayTrack:
      execPlayTrack(command.index,command.catalogueRevision);
      break;
    case Tab5AudioCommandType::PlayPath:
      execPlayPath(String(command.text));
      break;
    case Tab5AudioCommandType::SeekPermille:
      execSeekPermille(static_cast<uint16_t>(command.index));
      break;
    case Tab5AudioCommandType::PlayStation:
      execPlayStation(command.index,command.catalogueRevision);
      break;
    case Tab5AudioCommandType::Next:
      execNext();
      break;
    case Tab5AudioCommandType::Previous:
      execPrevious();
      break;
    case Tab5AudioCommandType::TogglePause:
      execTogglePause();
      break;
    case Tab5AudioCommandType::Stop:
      execStop();
      break;
    case Tab5AudioCommandType::StartRecording:
      execStartRecording();
      break;
    case Tab5AudioCommandType::StopRecording:
      execStopRecording();
      break;
    case Tab5AudioCommandType::WifiScan:
      execStartWifiScan();
      break;
    case Tab5AudioCommandType::WifiConnect:
      execConnectWifi(String(command.text), String(command.secret),
                      command.flag);
      break;
    case Tab5AudioCommandType::WifiEnable:
      execSetWifiEnabled(command.flag);
      break;
    case Tab5AudioCommandType::WifiForget:
      execForgetWifi();
      break;
    case Tab5AudioCommandType::None:
    default:
      break;
  }
}

bool Tab5AudioEngine::startAudioTask(bool essential) {
  // UI service is the sole starter. Commands may be posted by either core,
  // but they never enter cache collection while holding an audio state lock.
  if (!_workerLifecycle.tryStart()) return true;
  if (!_stateMutex || !_commandQueue) {
    _workerLifecycle.failedStart();
    return false;
  }
  const auto budget = Tab5Resources::TaskBudget(16U * 1024U);
  auto admission = essential
      ? tab5ReserveMemory(Tab5Resources::Owner::Audio, budget, "tache audio")
      : Tab5MemoryManager.reserve(Tab5Resources::Owner::Audio, budget);
  if (!admission) {
    _workerLifecycle.failedStart();
    return false;
  }
  TaskHandle_t task = nullptr;
  const BaseType_t result = tab5CreateAdmittedTask(admission,
      audioTaskEntry, "tab5_audio", 16U * 1024U, this,
      DECODER_TASK_PRIORITY, &task, _audioCore);
  if (result != pdPASS) {
    _workerLifecycle.failedStart();
    return false;
  }
  _audioTask.store(task, std::memory_order_release);
  _workerLifecycle.started();
  return true;
}

bool Tab5AudioEngine::begin(bool sdAvailable, uint8_t initialVolume) {
  if (!_stateMutex) _stateMutex = xSemaphoreCreateRecursiveMutex();
  if (!_snapshotMutex) _snapshotMutex = xSemaphoreCreateMutex();
  if (!_commandQueue) {
    // Queue payload is ordinary CPU data, never an I2S DMA buffer. Keep only
    // the FreeRTOS queue control in internal RAM (no ISR accesses this queue).
    _commandStorage = static_cast<uint8_t*>(heap_caps_calloc(
        12, sizeof(Tab5AudioCommand), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (_commandStorage) _commandQueue = xQueueCreateStatic(
        12, sizeof(Tab5AudioCommand), _commandStorage, &_commandQueueControl);
  }
  if (!_stateMutex || !_snapshotMutex || !_commandQueue) return false;
  g_audioCommandQueue = _commandQueue;
  g_audioMemoryPriority = &_memoryPriority;
  _sdAvailable = sdAvailable;
  _volume = initialVolume;

  // begin() est appelé depuis setup(), donc depuis le cœur qui exécutera la
  // boucle Arduino et LVGL. L'autre cœur est réservé à la chaîne audio.
  _uiCore = xPortGetCoreID();
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
  _audioCore = _uiCore;
#else
  _audioCore = (_uiCore == 0) ? 1 : 0;
#endif
  g_networkTaskCore = _uiCore;

  TAB5_LOG("[audio] coeur UI %d, coeur audio %d\n",
           (int)_uiCore, (int)_audioCore);
  if (!Tab5AudioDevice.begin(
          _volume, _audioCore, static_cast<uint8_t>(SPEAKER_TASK_PRIORITY))) {
    setStatus("Initialisation du pilote audio impossible");
    return false;
  }
  _output = new (std::nothrow) AudioOutputTab5Speaker(&Tab5AudioDevice);
  if (_output) _output->setMasterVolume(_volume);
  if (!_output) {
    setStatus("Mémoire insuffisante pour l’audio");
    return false;
  }

  // FAT traversal and the P4/C6 Wi-Fi handshake must not hold up the desktop.
  // The catalogues are loaded when the media page is first opened.
  addFallbackStations();
  _libraryLoaded = false;
  _stationsLoaded = false;
  loadWifiCredentials();
  _wifiEnabled = false;
  tab5NetworkSetEnabled(false);
  setStatus(_sdAvailable ? "Prêt" : "Carte SD absente");
  publishSnapshot();
  // The worker is created by UI service only when work is queued.
  if (_wifiAutoStart && _wifiSsid.length()) setWifiEnabled(true);
  return true;
}

void Tab5AudioEngine::reload() {
  Tab5StorageUse storage{true};
  if (!storage) return;
  StateGuard guard(*this);
  if (!guard) return;
  if (_sdAvailable) ensureAudioFiles();
  reloadLibrary();
  reloadStationsPreservingPlayback();
  _stationsLoaded = true;
  _uiDirty = true;
}

void Tab5AudioEngine::ensureLibraryLoaded() {
  Tab5StorageUse storage{true};
  if (!storage) return;
  StateGuard guard(*this);
  if (!guard) return;
  if (_libraryLoaded && _stationsLoaded) return;
  if (_sdAvailable) ensureAudioFiles();
  if (!_libraryLoaded) reloadLibrary();
  if (!_stationsLoaded) {
    reloadStationsPreservingPlayback();
    _stationsLoaded = true;
  }
  _uiDirty = true;
}

size_t Tab5AudioEngine::releaseIdleCatalogues() {
  // Exclude worker mutations without touching its decoder/file/socket. Music
  // needs its album list; radio needs its station list. The other catalogue
  // is reloadable even while playback continues in the background.
  Tab5StorageRecovery exclusive;
  if (!exclusive) return 0;
  StateGuard guard(*this, 0);
  if (!guard || _mode == Tab5AudioMode::Recording || _priorityCommandPending ||
      (_commandQueue && uxQueueMessagesWaiting(_commandQueue))) return 0;
  const size_t before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  if (_mode != Tab5AudioMode::Local) {
    Tab5CatalogueChange change(_trackCatalogueRevision);
    std::vector<String>().swap(_tracks);
    _libraryLoaded = false;
    _trackIndex = 0;
  }
  if (_mode != Tab5AudioMode::Radio) {
    Tab5CatalogueChange change(_stationCatalogueRevision);
    std::vector<Tab5AudioStation>().swap(_stations);
    _stationsLoaded = false;
    _stationIndex = 0;
  }
  const size_t after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  return after > before ? after - before : 0;
}

void Tab5AudioEngine::ensureAudioFiles() {
  if (!tab5FS.exists(AUDIO_ROOT)) tab5FS.mkdir(AUDIO_ROOT);
  if (!tab5FS.exists(RECORD_ROOT)) tab5FS.mkdir(RECORD_ROOT);

  if (!tab5FS.exists(RADIO_FILE) && !tab5FS.exists(LEGACY_RADIO_FILE)) {
    File file = tab5FS.open(RADIO_FILE, FILE_WRITE);
    if (file) {
      file.println("{\"radios\": []}");
      file.close();
    }
  }

}

void Tab5AudioEngine::reloadLibrary() {
  // Appelée depuis l'interface comme depuis la tâche audio : le mutex
  // récursif protège _tracks dans les deux cas.
  Tab5StorageUse storage{true};
  if (!storage) return;
  StateGuard guard(*this);
  if (!guard) return;
  Tab5CatalogueChange change(_trackCatalogueRevision);
  _tracks.clear();
  _scanEntries = 0;
  if (_sdAvailable) {
    scanFolder(AUDIO_ROOT, 0);
    if (tab5FS.exists(LEGACY_MP3_ROOT)) scanFolder(LEGACY_MP3_ROOT, 0);
    scanFolder(RECORD_ROOT, 0);
  }
  std::sort(_tracks.begin(), _tracks.end(), [](const String& a, const String& b) {
    return strcasecmp(a.c_str(), b.c_str()) < 0;
  });
  if (_trackIndex >= _tracks.size()) _trackIndex = 0;
  if (_scanEntries >= MAX_AUDIO_SCAN_ENTRIES) {
    Tab5DebugSerial.println("[Audio] parcours limité à 4096 entrées examinées");
    if (_mode == Tab5AudioMode::Idle)
      setStatus("Catalogue audio limité à 4096 entrées examinées");
  }
  _libraryLoaded = true;
  _uiDirty = true;
}

void Tab5AudioEngine::scanFolder(const String& folder, uint8_t depth) {
  if (depth > MAX_AUDIO_DEPTH || _tracks.size() >= MAX_AUDIO_ITEMS ||
      _scanEntries >= MAX_AUDIO_SCAN_ENTRIES ||
      !tab5FS.exists(folder.c_str())) return;
  File directory = tab5FS.open(folder.c_str(), FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return;
  }

  File entry = directory.openNextFile();
  while (entry && _tracks.size() < MAX_AUDIO_ITEMS &&
         _scanEntries < MAX_AUDIO_SCAN_ENTRIES) {
    ++_scanEntries;
    // Count every entry, including non-audio files and subdirectories. A
    // large unrelated tree must not monopolise the UI task or starve idle.
    if ((_scanEntries & 15U) == 0) vTaskDelay(1);
    String path = entry.path();
    if (!path.startsWith("/")) path = folder + "/" + path;
    const bool directoryEntry = entry.isDirectory();
    entry.close();
    if (directoryEntry) scanFolder(path, depth + 1);
    else if (isAudioPath(path)) _tracks.push_back(path);
    entry = directory.openNextFile();
  }
  if (entry) entry.close();
  directory.close();
}

void Tab5AudioEngine::addFallbackStations() {
  Tab5CatalogueChange change(_stationCatalogueRevision);
  _stations.clear();
}

void Tab5AudioEngine::loadStations() {
  Tab5CatalogueChange change(_stationCatalogueRevision);
  _stations.clear();
  const char* catalogue = nullptr;
  if (_sdAvailable) {
    if (tab5FS.exists(RADIO_FILE)) catalogue = RADIO_FILE;
    else if (tab5FS.exists(LEGACY_RADIO_FILE)) catalogue = LEGACY_RADIO_FILE;
  }
  if (!_sdAvailable || !catalogue) {
    return;
  }

  File file = tab5FS.open(catalogue, FILE_READ);
  if (!file || file.isDirectory() || file.size() > MAX_RADIO_CATALOGUE_BYTES) {
    if (file) file.close();
    return;
  }
  JsonDocument document(tab5JsonAllocator());
  JsonDocument fields(tab5JsonAllocator());
  auto wanted = fields["radios"][0].to<JsonObject>();
  for (const char* key : {"enabled", "name", "style", "age", "url", "codec"}) wanted[key] = true;
  // Both {"radios":[...]} and the legacy root array are accepted. Filtering
  // root arrays separately keeps unused provenance/tags out of the PSRAM DOM.
  int first = file.peek();
  while (first == ' ' || first == '\t' || first == '\r' || first == '\n') { file.read(); first = file.peek(); }
  const DeserializationError error = first == '['
      ? deserializeJson(document, file, DeserializationOption::Filter(fields["radios"].as<JsonVariantConst>()))
      : deserializeJson(document, file, DeserializationOption::Filter(fields));
  file.close();
  if (error) {
    return;
  }

  JsonArray entries = document["radios"].as<JsonArray>();
  if (entries.isNull() && document.is<JsonArray>()) entries = document.as<JsonArray>();
  if (entries.isNull()) {
    return;
  }

  for (JsonObject item : entries) {
    if (_stations.size() >= MAX_RADIO_STATIONS) break;
    if (!(item["enabled"] | true)) continue;
    const char* name = item["name"] | "";
    const char* style = item["style"] | "";
    const char* url = item["url"] | "";
    if (!style[0]) style = item["age"] | "Autres";
    if (name[0] && url[0]) _stations.push_back({String(name), String(style), String(url),
        tab5RadioCodecHint(item["codec"] | "")});
  }
  std::stable_sort(_stations.begin(), _stations.end(),
      [](const Tab5AudioStation& a, const Tab5AudioStation& b) {
        const int category = strcasecmp(a.style.c_str(), b.style.c_str());
        if (category) return category < 0;
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
      });
  if (_stationIndex >= _stations.size()) _stationIndex = 0;
}

void Tab5AudioEngine::reloadStationsPreservingPlayback() {
  const String playingUrl = _selectedRadioUrl;
  loadStations();
  _stationIndex = 0;
  for (size_t i = 0; i < _stations.size(); ++i) {
    if (_stations[i].url == playingUrl) {
      _stationIndex = i;
      break;
    }
  }
}

bool Tab5AudioEngine::loadWifiCredentials() {
  _wifiSsid = "";
  _wifiPassword = "";
  Preferences preferences;
  if (!preferences.begin(WIFI_PREFS_NAMESPACE, true)) return false;
  _wifiSsid = preferences.getString("ssid", "");
  _wifiPassword = preferences.getString("password", "");
  _wifiAutoStart = preferences.getBool("auto", false);
  preferences.end();
  return _wifiSsid.length() > 0;
}

bool Tab5AudioEngine::saveWifiCredentials() {
  if (!_wifiSsid.length()) return false;
  Preferences preferences;
  if (!preferences.begin(WIFI_PREFS_NAMESPACE, false)) return false;
  const bool ok = preferences.putString("ssid", _wifiSsid) > 0 &&
                  preferences.putString("password", _wifiPassword) > 0;
  preferences.end();
  return ok;
}

size_t Tab5AudioEngine::wifiNetworkCount() const {
  StateGuard guard(*this);
  return guard ? _wifiNetworks.size() : 0;
}

bool Tab5AudioEngine::wifiNetwork(size_t index, Tab5WifiNetwork& result) const {
  StateGuard guard(*this);
  if (!guard || index >= _wifiNetworks.size()) return false;
  result = _wifiNetworks[index];
  return true;
}

bool Tab5AudioEngine::wifiScanRunning() const {
  StateGuard guard(*this);
  return guard && (_wifiScanQueued || _wifiScanRunning.load());
}

uint32_t Tab5AudioEngine::wifiScanRevision() const {
  StateGuard guard(*this);
  return guard ? _wifiScanRevision : 0;
}

String Tab5AudioEngine::wifiScanStatus() const {
  StateGuard guard(*this);
  return guard ? _wifiScanStatus : String();
}

void Tab5AudioEngine::setWifiScanStatus(const char* message, bool clearNetworks) {
  StateGuard guard(*this);
  if (!guard) return;
  if (clearNetworks) _wifiNetworks.clear();
  _wifiScanStatus = message;
  ++_wifiScanRevision;
  _uiDirty = true;
}

bool Tab5AudioEngine::hasSavedWifi() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::hasSavedWifi);
}

String Tab5AudioEngine::savedWifiSsid() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::savedWifiSsid);
}

bool Tab5AudioEngine::wifiEnabled() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::wifiEnabled);
}

bool Tab5AudioEngine::wifiAutoStart() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::wifiAutoStart);
}

bool Tab5AudioEngine::wifiConnecting() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::wifiConnecting);
}

bool Tab5AudioEngine::execStartWifiScan() {
  {
    StateGuard guard(*this);
    if (!guard) return false;
    _wifiScanQueued = false;
    if (_wifiScanRunning) return true;
    // Keep controls unavailable while the driver starts, without holding the
    // state mutex across RPC/network calls. This worker also services results.
    _wifiScanRunning = true;
  }
  const auto rejected = [this](const char* message) {
    _wifiScanRunning = false;
    setWifiScanStatus(message, true);
    setStatus(message);
    return false;
  };
  if (tab5FileShareBusy()) return rejected("Arrêtez le partage de fichiers avant de modifier le Wi-Fi");
  if (_mode == Tab5AudioMode::Recording || _wifiPending || tab5NetworkBusy() || tab5NetworkMediaPending()) {
    return rejected("Scan différé : connexion ou lecture en cours");
  }
  Tab5Compatibility::beforeWifiStart();
  if (!WiFi.mode(WIFI_STA)) return rejected("Recherche Wi-Fi impossible");
  _wifiEnabled = true;
  tab5NetworkSetEnabled(true);
  if (WiFi.setSleep(false)) _wifiSleeping = false;
  WiFi.scanDelete();
  setWifiScanStatus("Recherche en cours…", true);
  const int16_t result = WiFi.scanNetworks(true, true);
  if (result == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    return rejected("Recherche Wi-Fi impossible");
  }
  _wifiScanRunning = true;
  ++_wifiScanRevision;
  if (_mode == Tab5AudioMode::Idle) setStatus("Recherche des réseaux Wi-Fi…");
  return true;
}

void Tab5AudioEngine::serviceWifiScan() {
  if (!_wifiScanRunning) return;
  const int16_t count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) return;
  _wifiScanRunning = false;
  std::vector<Tab5WifiNetwork> found;
  if (count > 0) {
    for (int16_t i = 0; i < count; ++i) {
      const String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;
      const int32_t rssi = WiFi.RSSI(i);
      const bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      bool exists = false;
      for (Tab5WifiNetwork& existing : found) {
        if (existing.ssid == ssid) {
          if (rssi > existing.rssi) {
            existing.rssi = rssi;
            existing.secured = secured;
          }
          exists = true;
          break;
        }
      }
      if (!exists) found.push_back({ssid, rssi, secured});
    }
    std::sort(found.begin(), found.end(),
              [](const Tab5WifiNetwork& a, const Tab5WifiNetwork& b) {
                return a.rssi > b.rssi;
              });
  }
  {
    StateGuard guard(*this);
    if (guard) _wifiNetworks.swap(found);
  }
  WiFi.scanDelete();
  ++_wifiScanRevision;
  if (count < 0) {
    setWifiScanStatus("Recherche Wi-Fi impossible");
    setStatus("Recherche Wi-Fi impossible");
    return;
  }
  setWifiScanStatus(count == 0 ? "Aucun réseau détecté" : "");
  if (_mode == Tab5AudioMode::Idle)
    setStatus(_wifiNetworks.empty() ? "Aucun réseau Wi-Fi détecté"
                                    : "Sélectionnez un réseau Wi-Fi");
}

bool Tab5AudioEngine::execConnectWifi(const String& ssid, const String& password,
                                      bool remember) {
  if (tab5FileShareBusy()) { setStatus("Arrêtez le partage de fichiers avant de modifier le Wi-Fi"); return false; }
  if (!ssid.length()) {
    setStatus("Sélectionnez un réseau Wi-Fi");
    return false;
  }
  if (_wifiPending && ssid == _wifiSsid &&
      (!password.length() || password == _wifiPassword)) {
    _saveWifiOnConnect = _saveWifiOnConnect || remember;
    return true;
  }
  if (WiFi.status() == WL_CONNECTED && ssid == WiFi.SSID() &&
      (!password.length() || password == _wifiPassword)) {
    if (remember) saveWifiCredentials();
    return true;
  }
  if (_mode == Tab5AudioMode::Radio && _streamSource) stopPlayback(true);
  tab5NetworkSetEnabled(false);
  Tab5NetworkTransition transition("connexion Wi-Fi", 20000);
  if (!transition) {
    tab5NetworkSetEnabled(_wifiEnabled);
    setStatus("Réseau encore occupé — réessayez");
    return false;
  }
  if (_wifiScanRunning) { WiFi.scanDelete(); _wifiScanRunning = false; setWifiScanStatus(""); }
  String effectivePassword = password;
  {
    StateGuard guard(*this);
    if (!guard) { tab5NetworkSetEnabled(_wifiEnabled); return false; }
    if (ssid == _wifiSsid && !password.length()) effectivePassword = _wifiPassword;
    _wifiSsid = ssid;
    _wifiPassword = effectivePassword;
  }
  _saveWifiOnConnect = remember;
  _playRadioAfterWifi = false;
  Tab5Compatibility::beforeWifiStart();
  if (!WiFi.mode(WIFI_STA)) {
    tab5NetworkSetEnabled(_wifiEnabled);
    setStatus("Échec de connexion Wi-Fi");
    return false;
  }
  _wifiEnabled = true;
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(),
             effectivePassword.length() ? effectivePassword.c_str() : nullptr);
  tab5NetworkSetEnabled(true);
  _wifiPending = true;
  _wifiStartedAt = millis();
  if (_mode == Tab5AudioMode::Idle || _mode == Tab5AudioMode::Radio)
    setStatus("Connexion à " + ssid + "…");
  return true;
}

bool Tab5AudioEngine::execSetWifiEnabled(bool enabled) {
  if (tab5FileShareBusy()) { setStatus("Arrêtez le partage de fichiers avant de modifier le Wi-Fi"); return false; }
  if (enabled) {
    if (_wifiPending) { _wifiEnabled = true; tab5NetworkSetEnabled(true); return true; }
    if (WiFi.status() == WL_CONNECTED) {
      _wifiEnabled = true;
      tab5NetworkSetEnabled(true);
      if (_mode == Tab5AudioMode::Idle)
        setStatus("Wi-Fi connecté à " + WiFi.SSID());
      return true;
    }
    String ssid;
    String password;
    {
      StateGuard guard(*this);
      if (guard) {
        ssid = _wifiSsid;
        password = _wifiPassword;
      }
    }
    if (ssid.length()) return execConnectWifi(ssid, password, false);
    Tab5Compatibility::beforeWifiStart();
    if (!WiFi.mode(WIFI_STA)) {
      setStatus("Échec de connexion Wi-Fi");
      return false;
    }
    _wifiEnabled = true;
    tab5NetworkSetEnabled(true);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    if (_mode == Tab5AudioMode::Idle)
      setStatus("Wi-Fi activé — choisissez un réseau");
    return true;
  }

  const bool wasRadio = _mode == Tab5AudioMode::Radio;
  const bool driverWasActive = _wifiEnabled || _wifiPending ||
                               _wifiScanRunning || wasRadio || WiFi.getMode() != WIFI_OFF;
  if (wasRadio) stopPlayback(true);
  tab5NetworkSetEnabled(false);
  Tab5NetworkTransition transition("arret Wi-Fi", 20000);
  if (!transition) { tab5NetworkSetEnabled(_wifiEnabled); setStatus("Arrêt réseau en attente — réessayez"); return false; }
  if (_wifiScanRunning) WiFi.scanDelete();
  _wifiScanRunning = false;
  _wifiPending = false;
  _saveWifiOnConnect = false;
  _playRadioAfterWifi = false;
  _radioReconnectPending = false;
  if (driverWasActive) {
    WiFi.disconnect(true, false);
    if (WiFi.getMode() != WIFI_OFF) {
      tab5NetworkSetEnabled(_wifiEnabled);
      setStatus("Arrêt réseau en attente — réessayez");
      return false;
    }
  }
  _wifiEnabled = false;
  setWifiScanStatus("Wi-Fi désactivé", true);
  if (_mode == Tab5AudioMode::Idle || wasRadio) setStatus("Wi-Fi désactivé");
  return true;
}

void Tab5AudioEngine::setWifiAutoStart(bool enabled) {
  StateGuard guard(*this);
  if (!guard) return;
  _wifiAutoStart = enabled;
  Preferences preferences;
  if (preferences.begin(WIFI_PREFS_NAMESPACE, false)) {
    preferences.putBool("auto", enabled);
    preferences.end();
  }
  _uiDirty = true;
}

void Tab5AudioEngine::execForgetWifi() {
  if (!execSetWifiEnabled(false)) return;
  Preferences preferences;
  if (preferences.begin(WIFI_PREFS_NAMESPACE, false)) {
    preferences.clear();
    preferences.end();
  }
  {
    StateGuard guard(*this);
    if (guard) {
      _wifiSsid = "";
      _wifiPassword = "";
    }
  }
  _wifiPending = false;
  _wifiScanRunning = false;
  _saveWifiOnConnect = false;
  _playRadioAfterWifi = false;
  _wifiEnabled = false;
  _wifiAutoStart = false;
  // execSetWifiEnabled already closed the driver after all clients stopped.
  if (_mode == Tab5AudioMode::Idle) setStatus("Réseau Wi-Fi oublié");
}

size_t Tab5AudioEngine::trackCount() const {
  StateGuard guard(*this);
  return guard ? _tracks.size() : 0;
}

String Tab5AudioEngine::trackPath(size_t index) const {
  StateGuard guard(*this);
  return guard && index < _tracks.size() ? _tracks[index] : String();
}

String Tab5AudioEngine::trackName(size_t index) const {
  StateGuard guard(*this);
  return guard && index < _tracks.size() ? baseName(_tracks[index]) : String();
}

size_t Tab5AudioEngine::stationCount() const {
  StateGuard guard(*this);
  return guard ? _stations.size() : 0;
}

bool Tab5AudioEngine::station(size_t index, Tab5AudioStation& result) const {
  StateGuard guard(*this);
  if (!guard || index >= _stations.size()) return false;
  result = _stations[index];
  return true;
}

bool Tab5AudioEngine::ensureMp3CodecBuffer() {
  if (_mp3CodecBuffer) return true;
  _mp3CodecBufferSize = AudioGeneratorMP3::preAllocSize();
  _mp3CodecBuffer = heap_caps_malloc(_mp3CodecBufferSize,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_mp3CodecBuffer) {
    setStatus("Mémoire insuffisante pour l’audio");
    tab5LogMemory("echec allocation MP3");
  }
  return _mp3CodecBuffer != nullptr;
}

bool Tab5AudioEngine::ensureAacCodecBuffer() {
  if (_aacCodecBuffer) return true;
  _aacCodecBuffer = heap_caps_calloc(1, aacWorkspaceBytes(),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_aacCodecBuffer) {
    setStatus("Mémoire insuffisante pour l’audio");
    tab5LogMemory("echec allocation AAC");
  }
  return _aacCodecBuffer != nullptr;
}

void Tab5AudioEngine::cleanupDecoder(bool preserveTransport) {
  if (_output) _output->stop();
  if (_mp3) {
    _mp3->stop();
    delete _mp3;
    _mp3 = nullptr;
  }
  if (_wav) {
    _wav->stop();
    delete _wav;
    _wav = nullptr;
  }
  if (_aac) {
    _aac->stop();
    delete _aac;
    _aac = nullptr;
  }
  if (_flac) {
    _flac->stop();
    delete _flac;
    _flac = nullptr;
  }
  heap_caps_free(_aacCodecBuffer);
  _aacCodecBuffer = nullptr;
  heap_caps_free(_mp3CodecBuffer);
  _mp3CodecBuffer = nullptr;
  _mp3CodecBufferSize = 0;
  if (_streamSource) {
    _streamSource->close();
    delete _streamSource;
    _streamSource = nullptr;
    _radioSource = nullptr;
  }
  if (_localSource) {
    _localSource->close();
    delete _localSource;
    _localSource = nullptr;
  }
  tab5NetworkSetMediaPending(false);
  tab5NetDiagnosticsRadioEnd();
  if (!preserveTransport) Tab5AudioDevice.release(Tab5AudioClient::Audio);
  delete _radioLease;
  _radioLease = nullptr;
}

void Tab5AudioEngine::stopPlayback(bool clearIntent, bool preserveTransport) {
  cleanupDecoder(preserveTransport && !clearIntent);
  _paused = false;
  _pauseStartedAt = 0;
  _playRadioAfterWifi = false;
  _radioReconnectPending = false;
  if (clearIntent) {
    _mode = Tab5AudioMode::Idle;
    _currentLocalPath = "";
    _playingRecording = false;
    _playStartedAt = 0;
    _localEstimatedSeconds = 0;
    setNowPlaying("");
  }
}

bool Tab5AudioEngine::startLocalDecoder(const String& path) {
  if (!_output || !_sdAvailable || !tab5FS.exists(path.c_str()) || !isAudioPath(path)) {
    setStatus("Fichier audio introuvable ou non pris en charge");
    return false;
  }

  stopPlayback(false, true);
  _localEstimatedSeconds = estimateLocalDuration(path);
  {
    StateGuard guard(*this);
    if (guard) Tab5AudioDevice.setVolume(_volume);
  }
  _localSource = new (std::nothrow) AudioFileSourceSD();
  if (!_localSource || !_localSource->open(path.c_str())) {
    cleanupDecoder();
    setStatus("Impossible d’ouvrir le fichier audio");
    return false;
  }

  String lower = path;
  lower.toLowerCase();
  bool started = false;
  if (lower.endsWith(".wav")) {
    _wav = new (std::nothrow) Tab5SeekableWavGenerator();
    started = _wav && _wav->begin(_localSource, _output);
  } else if (lower.endsWith(".aac")) {
    if (!ensureAacCodecBuffer()) { cleanupDecoder(); return false; }
    _aac = new (std::nothrow) Tab5SeekableAacGenerator(
        _aacCodecBuffer, aacWorkspaceBytes());
    started = _aac && _aac->begin(_localSource, _output);
  } else if (lower.endsWith(".flac")) {
    _flac = new (std::nothrow) Tab5SeekableFlacGenerator();
    started = _flac && _flac->begin(_localSource, _output);
  } else {
    if (!ensureMp3CodecBuffer()) { cleanupDecoder(); return false; }
    _mp3 = new (std::nothrow) Tab5SeekableMp3Generator(
        _mp3CodecBuffer, _mp3CodecBufferSize);
    started = _mp3 && _mp3->begin(_localSource, _output);
  }
  if (!started) {
    cleanupDecoder();
    setStatus("Format audio illisible");
    return false;
  }
  if (_wav) {
    const uint32_t wavSeconds =
        static_cast<Tab5SeekableWavGenerator*>(_wav)->durationSeconds();
    if (wavSeconds) _localEstimatedSeconds = wavSeconds;
  }
  _mode = Tab5AudioMode::Local;
  _paused = false;
  _currentLocalPath = path;
  _playingRecording = isRecordingPath(path);
  _currentAlbumFolder = _playingRecording ? String() : parentFolder(path);
  _playStartedAt = millis();
  _pauseStartedAt = 0;
  setNowPlaying(baseName(path));
  setStatus("Lecture locale");
  return true;
}

bool Tab5AudioEngine::execPlayPath(const String& path) {
  {
    StateGuard guard(*this);
    if (guard) {
      for (size_t i = 0; i < _tracks.size(); ++i) {
        if (_tracks[i] == path) {
          _trackIndex = i;
          break;
        }
      }
    }
  }
  return startLocalDecoder(path);
}

bool Tab5AudioEngine::execSeekPermille(uint16_t permille) {
  if (_mode != Tab5AudioMode::Local || !_localSource || !decoderRunning() ||
      !_currentLocalPath.length()) {
    setStatus("Déplacement réservé aux fichiers locaux");
    return false;
  }
  if (permille > 1000) permille = 1000;

  String lower = _currentLocalPath;
  lower.toLowerCase();
  bool moved = false;

  // Efface les quelques millisecondes PCM déjà mises en file, sinon l'ancien
  // passage reste audible juste après le déplacement.
  Tab5AudioDevice.stop(Tab5AudioClient::Audio);
  if (lower.endsWith(".mp3") && _mp3) {
    const uint32_t size = _localSource->getSize();
    uint32_t target = static_cast<uint32_t>(
        static_cast<uint64_t>(size) * permille / 1000ULL);
    if (target >= size && size) target = size - 1;
    moved = static_cast<Tab5SeekableMp3Generator*>(_mp3)->seekToByte(target);
  } else if (lower.endsWith(".aac") && _aac) {
    const uint32_t size = _localSource->getSize();
    uint32_t target = static_cast<uint32_t>(
        static_cast<uint64_t>(size) * permille / 1000ULL);
    if (target >= size && size) target = size - 1;
    moved = static_cast<Tab5SeekableAacGenerator*>(_aac)->seekToByte(target);
  } else if (lower.endsWith(".wav") && _wav) {
    moved = static_cast<Tab5SeekableWavGenerator*>(_wav)
        ->seekPermille(permille);
  } else if (lower.endsWith(".flac") && _flac) {
    moved = static_cast<Tab5SeekableFlacGenerator*>(_flac)
        ->seekPermille(permille, _localEstimatedSeconds);
  }

  if (!moved) {
    setStatus("Déplacement impossible pour ce fichier");
    return false;
  }

  const uint32_t now = millis();
  const uint32_t targetSeconds = _localEstimatedSeconds
      ? static_cast<uint32_t>(
          static_cast<uint64_t>(_localEstimatedSeconds) * permille / 1000ULL)
      : 0;
  _playStartedAt = now - targetSeconds * 1000U;
  if (_paused) _pauseStartedAt = now;
  _uiDirty = true;
  setStatus("Lecture locale");
  TAB5_LOG("[audio] déplacement local %u/1000 (%lus)\n",
           static_cast<unsigned>(permille),
           static_cast<unsigned long>(targetSeconds));
  return true;
}

bool Tab5AudioEngine::execPlayTrack(size_t index, uint32_t revision) {
  String path;
  {
    StateGuard guard(*this);
    if (!guard) return false;
    if(revision && revision!=trackCatalogueRevision()){setStatus("Choisissez un album");return false;}
    if (index >= _tracks.size()) {
      setStatus("Aucun fichier audio");
      return false;
    }
    _trackIndex = index;
    path = _tracks[index];
  }
  return startLocalDecoder(path);
}

bool Tab5AudioEngine::startRadioNow(size_t index, uint32_t revision) {
  if (!_output || WiFi.status() != WL_CONNECTED) return false;
  // La station est copiée sous verrou court, puis le verrou est rendu : la
  // poignée de main TLS et le préchargement se déroulent sans bloquer la
  // moindre lecture de l'interface.
  Tab5AudioStation station;
  {
    StateGuard guard(*this);
    if (!guard) return false;
    if(revision && revision!=stationCatalogueRevision()){setStatus("Choisissez une catégorie radio");return false;}
    if(_selectedRadioUrl.length()){
      index=_stations.size();
      for(size_t i=0;i<_stations.size();++i)if(_stations[i].url==_selectedRadioUrl){index=i;break;}
    }
    if(index>=_stations.size()){setStatus("Choisissez une catégorie radio");return false;}
    station = _stations[index];
    _stationIndex=index;
  }
  stopPlayback(false, true);
  tab5NetworkSetMediaPending(true);
  // Finish the preceding transaction before the I2S/receiver startup peak.
  // Successful setup later shares the established stream; cleanup releases it.
  _radioLease = new (std::nothrow) Tab5NetworkLease("radio", 20000,
      [](void*) { return !tab5NetworkEnabled() || audioCommandPending(); });
  if (!_radioLease || !*_radioLease) {
    _radioStartCancelled = audioCommandPending();
    cleanupDecoder();
    setStatus("Réseau occupé — touchez pour réessayer");
    return false;
  }
  Tab5Resources::AdmissionReport report;
  // A redirect or an opaque endpoint may advertise a different codec than
  // its URL. Reserve the larger supported workspace before opening the stream.
  const size_t codecBytes = std::max(aacWorkspaceBytes(),
      static_cast<size_t>(AudioGeneratorMP3::preAllocSize()));
  const size_t availablePsram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const size_t ringBytes=Tab5Resources::radioCapacity(availablePsram,availablePsram);
  if (!ringBytes) {
    cleanupDecoder();
    setStatus("Mémoire insuffisante pour l’audio");
    return false;
  }
  const bool outputRunning=Tab5AudioDevice.owner()==Tab5AudioClient::Audio &&
      M5.Speaker.isRunning();
  auto budget = Tab5Resources::radioBudget(station.url.startsWith("https://"),
      ringBytes,codecBytes,outputRunning);
  const auto speaker = M5.Speaker.config();
  // The policy uses M5Unified's 256x8 baseline. An optional larger DMA
  // configuration must also cover its real stereo buffers and worker stack.
  const size_t dmaBytes = speaker.dma_buf_len * speaker.dma_buf_count * 4U;
  const size_t speakerStack = 1280U + speaker.dma_buf_len * sizeof(uint32_t);
  const size_t nominalSpeakerBytes = 8U * 1024U + 2304U;
  if (!outputRunning && dmaBytes + speakerStack > nominalSpeakerBytes)
    budget.internal += dmaBytes + speakerStack - nominalSpeakerBytes;
  auto admission = tab5ReserveMemory(Tab5Resources::Owner::Audio, budget,
      "demarrage radio", &report);
  if (!admission) {
    cleanupDecoder();
    char reason[192];
    Tab5Resources::describeAdmission(report, reason, sizeof(reason));
    setStatus(String("Radio : ") + reason);
    return false;
  }
  size_t internalBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  WiFi.setSleep(false);
  {
    StateGuard guard(*this);
    if (guard) Tab5AudioDevice.setVolume(_volume);
  }
  // Allocate the real I2S resources before TCP starts receiving. The 0.4.26
  // trace failed here after the first burst had consumed the DMA heap.
  if (!Tab5AudioDevice.claim(Tab5AudioClient::Audio, 250)) {
    cleanupDecoder();
    setStatus("Sortie audio indisponible — réessayez après l’arrêt du média");
    return false;
  }
  const size_t internalAfterSpeaker = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  admission.allocated(internalBefore > internalAfterSpeaker
      ? internalBefore - internalAfterSpeaker : 0, 0);
  internalBefore = internalAfterSpeaker;
  _localEstimatedSeconds = 0;
  _stationCategory = station.style;
  tab5NetDiagnosticsService();
  TAB5_LOG("[radio] ouverture du flux distant\n");
  tab5LogHeap("avant ouverture");
  tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage::HttpOpen);
  Tab5BufferedRadioSource* radio = new (std::nothrow) Tab5BufferedRadioSource();
  _streamSource = radio;
  _radioSource = radio;
  if (radio) radio->open(station.url.c_str(),ringBytes);
  tab5LogHeap("apres ouverture");
  if (!_streamSource || !_streamSource->isOpen()) {
    TAB5_LOG("[radio] flux non ouvert\n");
    _radioStartCancelled = audioCommandPending();
    cleanupDecoder();
    // Un transport P4/C6 bloque ne doit pas etre martele par une nouvelle
    // poignee de main TLS toutes les 2,5 secondes. L'utilisateur peut relancer
    // explicitement la station apres avoir lu le diagnostic serie.
    setStatus("Flux radio inaccessible — touchez pour réessayer");
    _radioReconnectPending = false;
    _radioReconnectAt = millis();
    return false;
  }
  const size_t internalAfterTransport = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  admission.allocated(internalBefore > internalAfterTransport
      ? internalBefore - internalAfterTransport : 0, radio->capacityBytes());
  tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage::DecoderStart);
  Tab5DebugSerial.printf("[radio][AUDIO] DEMARRAGE_DECODEUR, pile min=%u octets\n",
      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  Tab5DebugSerial.flush();
  bool decoderStarted = false;
  const Tab5RadioCodec codec = radio->codec(station.codec, station.url.c_str());
  if (codec == Tab5RadioCodec::Unsupported) {
    cleanupDecoder();
    setStatus("Flux MP3/AAC indisponible — touchez pour réessayer");
    return false;
  }
  if (codec == Tab5RadioCodec::Aac) {
    if (!ensureAacCodecBuffer()) { cleanupDecoder(); return false; }
    _aac = new (std::nothrow) Tab5SeekableAacGenerator(
        _aacCodecBuffer, aacWorkspaceBytes());
    decoderStarted = _aac && _aac->begin(_streamSource, _output);
  } else {
    if (!ensureMp3CodecBuffer()) { cleanupDecoder(); return false; }
    _mp3 = new (std::nothrow) AudioGeneratorMP3(_mp3CodecBuffer, _mp3CodecBufferSize);
    decoderStarted = _mp3 && _mp3->begin(_streamSource, _output);
  }
  TAB5_LOG("[radio] decodeur %s\n", decoderStarted ? "demarre" : "refuse");
  Tab5DebugSerial.flush();
  tab5LogHeap("decodeur pret");
  if (!decoderStarted) {
    cleanupDecoder();
    setStatus("Flux MP3/AAC indisponible — touchez pour réessayer");
    _radioReconnectPending = false;
    _radioReconnectAt = millis();
    return false;
  }
  admission.release();
  // Setup peaks are now in the live heap sample. Keep this established stream
  // registered for Wi-Fi shutdown, while allowing one ordinary transaction.
  _radioLease->shareAfterSetup();
  tab5NetworkSetMediaPending(false);
  _mode = Tab5AudioMode::Radio;
  _paused = false;
  _currentLocalPath = "";
  _playingRecording = false;
  _playStartedAt = millis();
  _pauseStartedAt = 0;
  _radioReconnectPending = false;
  tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage::Playing);
  setNowPlaying(station.name);
  setStatus(station.style + "  •  radio Internet");
  return true;
}

bool Tab5AudioEngine::execPlayStation(size_t index, uint32_t revision) {
  _radioStartCancelled = false;
  Tab5AudioStation station;
  {
    StateGuard guard(*this);
    if (!guard) return false;
    if(revision && revision!=stationCatalogueRevision()){setStatus("Choisissez une catégorie radio");return false;}
    revision=stationCatalogueRevision();
    if (index >= _stations.size()) {
      setStatus("Aucune radio");
      return false;
    }
    station = _stations[index];
    _selectedRadioUrl=station.url;
    _stationIndex=index;
    _stationCategory=station.style;
  }
  if (!_wifiEnabled) {
    setStatus("Activez le Wi-Fi dans Réglages");
    return false;
  }
  const bool immediateStart = WiFi.status() == WL_CONNECTED;
  stopPlayback(false, immediateStart);
  _mode = Tab5AudioMode::Radio;
  setNowPlaying(station.name);
  setStatus("Connexion à " + station.name + "…");
  // Publication immédiate : l'interface affiche la station choisie pendant
  // que l'ouverture du flux se poursuit ici, sur la tâche audio.
  {
    StateGuard guard(*this, pdMS_TO_TICKS(10));
    if (guard) publishSnapshot();
  }

  _radioRetryCount = 0;
  switch (Tab5Connection::forStation(WiFi.status() == WL_CONNECTED, _wifiPending)) {
    case Tab5Connection::WifiAction::Ready: {
      const bool started = startRadioNow(index, revision);
      if (!started) cleanupDecoder();
      return started;
    }
    case Tab5Connection::WifiAction::Wait:
      if (immediateStart) cleanupDecoder();
      _playRadioAfterWifi = true;
      setStatus("En attente du Wi-Fi…");
      return true;
    case Tab5Connection::WifiAction::Start:
      if (immediateStart) cleanupDecoder();
      break;
  }

  String ssid;
  String password;
  {
    StateGuard guard(*this);
    if (!guard) return false;
    if (!_wifiSsid.length() && !loadWifiCredentials()) {
      setStatus("Sélectionnez un réseau dans Réglages");
      return false;
    }
    ssid = _wifiSsid;
    password = _wifiPassword;
  }
  const bool started = execConnectWifi(ssid, password, false);
  _playRadioAfterWifi = started;
  return started;
}

bool Tab5AudioEngine::resumeSelectedStation() {
  size_t index=0;uint32_t revision=0;
  {
    StateGuard guard(*this);
    if(!guard)return false;
    index=_stations.size();
    for(size_t i=0;i<_stations.size();++i)
      if(_stations[i].url==_selectedRadioUrl){index=i;break;}
    if(index==_stations.size()){setStatus("Choisissez une catégorie radio");return false;}
    revision=stationCatalogueRevision();
  }
  return execPlayStation(index,revision);
}

bool Tab5AudioEngine::decoderRunning() const {
  return (_mp3 && _mp3->isRunning()) || (_aac && _aac->isRunning()) ||
         (_wav && _wav->isRunning()) || (_flac && _flac->isRunning());
}

bool Tab5AudioEngine::decoderLoop() {
  if (_mp3 && _mp3->isRunning()) return _mp3->loop();
  if (_aac && _aac->isRunning()) return _aac->loop();
  if (_wav && _wav->isRunning()) return _wav->loop();
  if (_flac && _flac->isRunning()) return _flac->loop();
  return false;
}

void Tab5AudioEngine::serviceTelemetry() {
#if TAB5_RADIO_TRACE
  if (_mode != Tab5AudioMode::Radio || _paused || !_radioSource || !_output)
    return;
  const uint32_t now = millis();
  if (now - _lastTelemetryAt < 5000) return;
  _lastTelemetryAt = now;

  // Une ligne toutes les cinq secondes. En plus de la continuite du flux, les
  // minima de RAM et les marges de pile permettent de distinguer une fuite,
  // une fragmentation et un debordement de pile d'une coupure reseau.
  //   file : encoded data remaining; a decreasing level alone cannot
  //          distinguish a stopped receiver from a sample-clock problem.
  //   attente : millisecondes passees a attendre le haut-parleur. S'il monte,
  //             le goulot est la sortie PCM.
  //   recharge: nombre de sous-alimentations reseau averees.
  const size_t buffered = _radioSource->bufferedBytes();
  const size_t capacity = _radioSource->capacityBytes();
  const size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const size_t internalMinimum =
      heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  const size_t internalLargest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const uint32_t audioStackFree = uxTaskGetStackHighWaterMark(nullptr);
  const uint32_t networkStackFree = _radioSource->networkStackFreeBytes();
  Tab5DebugSerial.printf(
      "[radio] t=%lus file=%u/%uKo blocs=%lu attente=%lums perdus=%lu "
      "recharge=%lu courts=%lu recus=%lu rx=%u %dHz->%uHz "
      "RAM=%uKo min=%uKo bloc=%uKo PSRAM=%uKo piles=%lu/%lu octets\n",
      (unsigned long)((now - _playStartedAt) / 1000UL),
      (unsigned)(buffered / 1024), (unsigned)(capacity / 1024),
      (unsigned long)_output->pushedBlocks(),
      (unsigned long)_output->waitTicks(),
      (unsigned long)_output->droppedBlocks(),
      (unsigned long)_radioSource->refillWaits(),
      (unsigned long)_radioSource->shortReads(),
      (unsigned long)_radioSource->receivedBytes(),
      (unsigned)_radioSource->receiving(),
      _output->sourceRate(),
      (unsigned)Tab5AudioDevice.outputRate(),
      (unsigned)(internalFree / 1024),
      (unsigned)(internalMinimum / 1024),
      (unsigned)(internalLargest / 1024),
      (unsigned)(psramFree / 1024),
      (unsigned long)audioStackFree,
      (unsigned long)networkStackFree);
#endif
}

bool Tab5AudioEngine::wifiPowerTarget(uint32_t now) {
  if (_wifiPending || _wifiScanRunning || tab5NetworkBusy() || tab5NetworkMediaPending())
    _networkLastBusy = now;
  return _wifiPowerSave.load() && _wifiEnabled &&
      WiFi.status() == WL_CONNECTED && now - _networkLastBusy >= 5000U;
}

void Tab5AudioEngine::serviceWifiPower() {
  const uint32_t now = millis();
  const bool save = wifiPowerTarget(now);
  if (save != _wifiSleeping && (!save || now - _wifiPowerAttemptAt >= 5000U)) {
    _wifiPowerAttemptAt = now;
    if (WiFi.setSleep(save)) _wifiSleeping = save;
  }
}

void Tab5AudioEngine::serviceWifiConnection() {
  if (_wifiPending) {
    if (WiFi.status() == WL_CONNECTED) {
      _wifiPending = false;
      WiFi.setSleep(false);
      WiFi.setAutoReconnect(true);
      if (_saveWifiOnConnect) saveWifiCredentials();
      _saveWifiOnConnect = false;
      const bool startRadio = _playRadioAfterWifi && !_paused && _mode == Tab5AudioMode::Radio;
      _playRadioAfterWifi = false;
      if (startRadio) startRadioNow(_stationIndex);
      else if (_mode == Tab5AudioMode::Idle)
        setStatus("Wi-Fi connecté à " + WiFi.SSID());
    } else if (millis() - _wifiStartedAt >= WIFI_TIMEOUT_MS) {
      const bool retry = _playRadioAfterWifi && _radioRetryCount > 0 &&
          _radioRetryCount < Tab5Connection::MaxRadioRetries && !_paused;
      _wifiPending = false;
      _saveWifiOnConnect = false;
      _playRadioAfterWifi = false;
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false, false);
      _radioReconnectPending = retry;
      _radioReconnectAt = millis();
      if (_mode == Tab5AudioMode::Idle || _mode == Tab5AudioMode::Radio)
        setStatus("Échec de connexion Wi-Fi");
    }
  }

}

void Tab5AudioEngine::serviceDecoder() {
  serviceWifiScan();
  // The priority owner must see stable released playback resources until its
  // lease ends. Wi-Fi state remains alive; pending user playback is deferred.
  if (_memoryPriority.state() == Tab5AudioPriorityControl::Held ||
      _memoryPriority.state() == Tab5AudioPriorityControl::Release) return;
  serviceWifiPower();
  serviceTelemetry();
  if (_mode == Tab5AudioMode::Recording) {
    serviceRecording();
    return;
  }
  serviceWifiConnection();

  if (_paused) return;
  if (decoderRunning()) {
    if (_radioRetryCount && _radioSource && _radioSource->receiving() &&
        Tab5Connection::elapsed(millis(), _playStartedAt, Tab5Connection::StablePlaybackMs))
      _radioRetryCount = 0;
    const bool decoding = decoderLoop();
    if (Tab5AudioDevice.failed()) {
      // A failed PCM sink is not EOF and must not advance the playlist or
      // repeatedly reconnect a healthy radio transport to a broken output.
      stopPlayback(true);
      setStatus("Sortie audio indisponible — réessayez après l’arrêt du média");
      return;
    }
    if (!decoding) {
      // Capturé avant cleanupDecoder(), qui remet _radioSource à nullptr.
      // Cette unique information suffit à distinguer proprement une fin de
      // WebRadio d'une fin de fichier local, sans état radio parallèle.
      const bool finishedRadio =
          _mode == Tab5AudioMode::Radio && _radioSource != nullptr;
      const bool finishedRecording = !finishedRadio &&
          _mode == Tab5AudioMode::Local && _playingRecording;
      const String finishedName = _nowPlaying;
      cleanupDecoder(_mode == Tab5AudioMode::Local && !finishedRecording);
      if (finishedRadio) {
        _radioReconnectPending = _wifiEnabled &&
            _radioRetryCount < Tab5Connection::MaxRadioRetries;
        _radioReconnectAt = millis();
        setStatus(_radioReconnectPending ? "Flux interrompu — reprise prévue" :
            "Radio arrêtée après trois échecs — touchez pour réessayer");
        TAB5_LOG("[radio] fin du flux, reconnexion programmée\n");
      } else if (finishedRecording) {
        _mode = Tab5AudioMode::Idle;
        _playingRecording = false;
        _playStartedAt = 0;
        setNowPlaying(finishedName);
        setStatus("Fin de l’enregistrement");
      } else if (_mode == Tab5AudioMode::Local) {
        if (!selectAdjacentTrack(1, false)) {
          Tab5AudioDevice.release(Tab5AudioClient::Audio);
          _mode = Tab5AudioMode::Idle;
          _playStartedAt = 0;
          setStatus("Fin de l’album");
        }
      }
    }
  } else if (_radioReconnectPending && !_wifiPending &&
             Tab5Connection::elapsed(millis(), _radioReconnectAt,
                                     Tab5Connection::retryDelay(_radioRetryCount))) {
    ++_radioRetryCount;
    _radioReconnectPending = false;
    if (WiFi.status() == WL_CONNECTED) {
      if (!startRadioNow(_stationIndex) && _wifiEnabled &&
          _radioRetryCount < Tab5Connection::MaxRadioRetries && !audioCommandPending()) {
        _radioReconnectPending = true;
        _radioReconnectAt = millis();
      }
    } else {
      String ssid, password;
      { StateGuard guard(*this); if (guard) { ssid = _wifiSsid; password = _wifiPassword; } }
      _playRadioAfterWifi = execConnectWifi(ssid, password, false);
    }
  }
}

void Tab5AudioEngine::serviceIdleMemoryPriority() {
  // UI service is the only starter. A stopped worker and an empty decoder
  // let it acknowledge a lease without allocating a 16 KiB task just to say
  // that no playback needs suspending. No command, file or network work here.
  StateGuard guard(*this, 0);
  if (!guard || _workerLifecycle.active() || _mode != Tab5AudioMode::Idle ||
      _localSource || _streamSource || _radioSource || _radioLease ||
      _mp3 || _wav || _aac || _flac || _recordFile ||
      _priorityResume || _priorityCommandPending) return;
  const uint32_t requested = _memoryPriority.begin();
  if (requested) _memoryPriority.acknowledge(requested, true);
  const uint32_t released = _memoryPriority.released();
  if (released) _memoryPriority.complete(released);
}

void Tab5AudioEngine::service() {
  Tab5AudioDevice.service();
  if (_workerLifecycle.active()) return;
  serviceIdleMemoryPriority();
  const uint32_t now = millis();
  if (_idleReleaseRequested.exchange(false, std::memory_order_acq_rel)) {
    // Do not immediately spend the reclaimed stack on an RSSI refresh while
    // another application is being admitted. User commands still take priority.
    _lastHousekeepingAttempt = now;
  }
  const auto priority = _memoryPriority.state();
  const bool essential = (_commandQueue && uxQueueMessagesWaiting(_commandQueue)) ||
      (priority != Tab5AudioPriorityControl::Idle &&
       priority != Tab5AudioPriorityControl::Held);
  // Only this UI service starts workers. The acquire above observes the
  // worker's final release, so ordinary engine fields have a single owner.
  const bool connected = _wifiEnabled && WiFi.status() == WL_CONNECTED;
  if (!connected) {
    _wifiSignalSampled = false;
    _wifiRssiCached = 0;
  }
  const bool save = wifiPowerTarget(now);  // Cached state only; no C6 RPC.
  const bool powerChange = connected && save != _wifiSleeping &&
      (!save || uint32_t(now - _wifiPowerAttemptAt) >= 5000U);
  const bool signalRequested = connected && _mode == Tab5AudioMode::Idle &&
      !_wifiPending && !_wifiScanRunning &&
      (!_wifiSignalSampled || (_wifiSignalRequested.load(std::memory_order_acquire) &&
        uint32_t(now - _lastWifiSignalAt) >= 5000U));
  const bool housekeeping = priority == Tab5AudioPriorityControl::Idle &&
      (powerChange || signalRequested) &&
      uint32_t(now - _lastHousekeepingAttempt) >= 5000U;
  if ((essential && (!_workerRetryPending || uint32_t(now - _lastWorkerAttempt) >= 500U)) ||
      (!essential && housekeeping)) {
    _lastWorkerAttempt = now;
    _lastHousekeepingAttempt = now;
    if (startAudioTask(essential)) {
      _workerRetryPending = false;
      return;
    }
    _workerRetryPending = essential;
    if (essential) setStatus("Mémoire insuffisante pour l’audio");
  }
  // The stopped lifecycle guarantees exclusive ownership of these fields.
  // WiFi.status() is cached; snapshot publication performs no C6 RPC, decode,
  // connection or file operation. Heavy work never falls back to the UI task.
  StateGuard guard(*this, 0);
  if (guard) publishSnapshot();
}

bool Tab5AudioEngine::selectAdjacentTrack(int direction, bool recordingsOnly) {
  const String album = _currentAlbumFolder.length()
      ? _currentAlbumFolder : parentFolder(_currentLocalPath);
  String chosen;
  {
    StateGuard guard(*this);
    if (!guard || _tracks.empty()) return false;
    if(_currentLocalPath.length()){
      size_t current=_tracks.size();
      for(size_t i=0;i<_tracks.size();++i)if(_tracks[i]==_currentLocalPath){current=i;break;}
      if(current==_tracks.size())return false;
      _trackIndex=current;
    }
    int candidate = static_cast<int>(_trackIndex) + (direction < 0 ? -1 : 1);
    while (candidate >= 0 && candidate < static_cast<int>(_tracks.size())) {
      const String& path = _tracks[static_cast<size_t>(candidate)];
      const bool matches = recordingsOnly
          ? isRecordingPath(path)
          : (!isRecordingPath(path) && parentFolder(path) == album);
      if (matches) {
        chosen = path;
        _trackIndex = static_cast<size_t>(candidate);
        break;
      }
      candidate += direction < 0 ? -1 : 1;
    }
  }
  if (!chosen.length()) return false;
  return startLocalDecoder(chosen);
}

bool Tab5AudioEngine::selectAdjacentStation(int direction) {
  int chosen = -1;uint32_t revision=0;
  {
    StateGuard guard(*this);
    if (!guard || _stations.empty()) return false;
    if(_selectedRadioUrl.length()){
      size_t current=_stations.size();
      for(size_t i=0;i<_stations.size();++i)if(_stations[i].url==_selectedRadioUrl){current=i;break;}
      if(current==_stations.size())return false;
      _stationIndex=current;
    }
    if(_stationIndex>=_stations.size())return false;
    revision=stationCatalogueRevision();
    const String category = _stationCategory.length()
        ? _stationCategory : _stations[_stationIndex].style;
    int candidate = static_cast<int>(_stationIndex) + (direction < 0 ? -1 : 1);
    while (candidate >= 0 && candidate < static_cast<int>(_stations.size())) {
      if (_stations[static_cast<size_t>(candidate)].style == category) {
        chosen = candidate;
        break;
      }
      candidate += direction < 0 ? -1 : 1;
    }
  }
  if (chosen < 0) return false;
  return execPlayStation(static_cast<size_t>(chosen),revision);
}

void Tab5AudioEngine::execPrevious() {
  bool haveStations = false;
  bool haveTracks = false;
  {
    StateGuard guard(*this);
    if (!guard) return;
    haveStations = !_stations.empty();
    haveTracks = !_tracks.empty();
  }
  if (_mode == Tab5AudioMode::Radio && haveStations) {
    if (!selectAdjacentStation(-1)) setStatus("Première radio de la catégorie");
  } else if (haveTracks) {
    if (!selectAdjacentTrack(-1, isRecordingPath(_currentLocalPath)))
      setStatus(_playingRecording ? "Premier enregistrement" : "Premier titre de l’album");
  }
}

void Tab5AudioEngine::execNext() {
  bool haveStations = false;
  bool haveTracks = false;
  {
    StateGuard guard(*this);
    if (!guard) return;
    haveStations = !_stations.empty();
    haveTracks = !_tracks.empty();
  }
  if (_mode == Tab5AudioMode::Radio && haveStations) {
    if (!selectAdjacentStation(1)) setStatus("Dernière radio de la catégorie");
  } else if (haveTracks) {
    if (!selectAdjacentTrack(1, isRecordingPath(_currentLocalPath)))
      setStatus(_playingRecording ? "Dernier enregistrement" : "Dernier titre de l’album");
  }
}

void Tab5AudioEngine::execTogglePause() {
  const bool radio = _mode == Tab5AudioMode::Radio;
  if (_mode != Tab5AudioMode::Local && !radio) return;
  if (radio && !_paused && (_playRadioAfterWifi || _radioReconnectPending || _radioStartCancelled)) {
    _playRadioAfterWifi = _radioReconnectPending = _radioStartCancelled = false;
    cleanupDecoder();
    _paused = true;
    setStatus("Pause");
    return;
  }
  if (Tab5Resources::retryRadioInsteadOfPause(radio, _paused, decoderRunning())) {
    if (!resumeSelectedStation()) cleanupDecoder();
    return;
  }
  _paused = !_paused;
  if (_paused) {
    _pauseStartedAt = millis();
    Tab5AudioDevice.stop(Tab5AudioClient::Audio);
    // Une radio en pause ne peut pas rester connectée : le flux continue
    // d'arriver et la file se remplit pour rien. On ferme, la reprise
    // rouvre le flux.
    if (radio) cleanupDecoder(true);
    setStatus("Pause");
  } else if (radio && !decoderRunning()) {
    _paused = false;
    _pauseStartedAt = 0;
    if (!resumeSelectedStation()) cleanupDecoder();
  } else {
    if (_pauseStartedAt && _playStartedAt)
      _playStartedAt += millis() - _pauseStartedAt;
    _pauseStartedAt = 0;
    setStatus(radio ? "Radio Internet" : "Lecture locale");
  }
  _uiDirty = true;
}

void Tab5AudioEngine::execStop() {
  if (_mode == Tab5AudioMode::Recording) execStopRecording();
  else {
    stopPlayback(true);
    setStatus("Arrêté");
  }
}

String Tab5AudioEngine::makeRecordingPath() const {
  time_t now = time(nullptr);
  struct tm local = {};
  if (now > 100000) localtime_r(&now, &local);
  char path[72];
  if (now > 100000) {
    snprintf(path, sizeof(path), "%s/REC_%04d%02d%02d_%02d%02d%02d.wav",
             RECORD_ROOT, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
             local.tm_hour, local.tm_min, local.tm_sec);
  } else {
    snprintf(path, sizeof(path), "%s/REC_%010lu.wav", RECORD_ROOT,
             static_cast<unsigned long>(millis()));
  }
  String result(path);
  uint8_t suffix = 1;
  while (tab5FS.exists(result.c_str()) && suffix < 100) {
    result = String(path);
    if (result.endsWith(".wav")) result.remove(result.length() - 4);
    result += "_" + String(suffix++) + ".wav";
  }
  return result;
}

bool Tab5AudioEngine::writeWavHeader(File& file, uint32_t dataBytes) {
  if (!file.seek(0)) return false;
  return file.write(reinterpret_cast<const uint8_t*>("RIFF"), 4) == 4 &&
      writeLe32(file, 36 + dataBytes) &&
      file.write(reinterpret_cast<const uint8_t*>("WAVE"), 4) == 4 &&
      file.write(reinterpret_cast<const uint8_t*>("fmt "), 4) == 4 &&
      writeLe32(file, 16) && writeLe16(file, 1) && writeLe16(file, 1) &&
      writeLe32(file, RECORD_SAMPLE_RATE) &&
      writeLe32(file, RECORD_SAMPLE_RATE * sizeof(int16_t)) &&
      writeLe16(file, sizeof(int16_t)) && writeLe16(file, 16) &&
      file.write(reinterpret_cast<const uint8_t*>("data"), 4) == 4 &&
      writeLe32(file, dataBytes);
}

bool Tab5AudioEngine::execStartRecording() {
  if (_mode == Tab5AudioMode::Recording) return false;
  if (!_sdAvailable) {
    setStatus("Carte SD requise pour enregistrer");
    return false;
  }
  stopPlayback(true);
  // Recording can also be launched without ever opening the media catalogue.
  if (!tab5FS.exists(RECORD_ROOT) && !tab5FS.mkdir(RECORD_ROOT)) {
    tab5StorageFault("dossier enregistrements", RECORD_ROOT);
    setStatus("Carte SD inaccessible pour enregistrer");
    return false;
  }
  // M5Unified's mic task reads I2S into its own internal buffer and copies
  // converted samples here; these three SD-write staging blocks are not DMA.
  _recordBuffer = static_cast<int16_t (*)[RECORD_SAMPLES]>(heap_caps_calloc(
      3, RECORD_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!_recordBuffer) {
    setStatus("Memoire insuffisante pour enregistrer");
    return false;
  }
  Tab5AudioDevice.suspend();
  auto micConfig = M5.Mic.config();
  micConfig.sample_rate = RECORD_SAMPLE_RATE;
  if (micConfig.magnification < RECORD_MIC_MAGNIFICATION)
    micConfig.magnification = RECORD_MIC_MAGNIFICATION;
  M5.Mic.config(micConfig);
  if (!M5.Mic.begin()) {
    heap_caps_free(_recordBuffer);
    _recordBuffer = nullptr;
    Tab5AudioDevice.resume();
    setStatus("Microphones indisponibles");
    return false;
  }

  _recordPath = makeRecordingPath();
  _recordFile = tab5FS.open(_recordPath.c_str(), FILE_WRITE);
  if (!_recordFile || !writeWavHeader(_recordFile, 0)) {
    if (_recordFile) _recordFile.close();
    M5.Mic.end();
    heap_caps_free(_recordBuffer);
    _recordBuffer = nullptr;
    Tab5AudioDevice.resume();
    setStatus("Création du fichier impossible");
    return false;
  }
  _recordDataBytes = 0;
  _recordQueueIndex = 0;
  _recordPendingCount = 0;
  _recordLevel = 0;
  _recordStartedAt = millis();
  _mode = Tab5AudioMode::Recording;
  _paused = false;
  setNowPlaying(baseName(_recordPath));
  setStatus("Enregistrement en cours");
  return true;
}

void Tab5AudioEngine::serviceRecording() {
  if (!_recordFile || !M5.Mic.isEnabled()) return;
  if (!M5.Mic.record(_recordBuffer[_recordQueueIndex], RECORD_SAMPLES,
                     RECORD_SAMPLE_RATE)) return;
  if (_recordPendingCount < 2) {
    ++_recordPendingCount;
  } else {
    const size_t completed = (_recordQueueIndex + 1) % 3;
    if (!writeRecordChunk(completed)) {
      finishRecording(false);
      setStatus("Erreur d’écriture sur la carte SD");
      return;
    }
  }
  _recordQueueIndex = (_recordQueueIndex + 1) % 3;
}

bool Tab5AudioEngine::writeRecordChunk(size_t bufferIndex) {
  const size_t bytes = RECORD_SAMPLES * sizeof(int16_t);
  if (_recordFile.write(
          reinterpret_cast<const uint8_t*>(_recordBuffer[bufferIndex]), bytes) != bytes)
    return false;
  _recordDataBytes += bytes;
  uint16_t peak = 0;
  for (size_t i = 0; i < RECORD_SAMPLES; i += 8) {
    const int32_t value = abs(static_cast<int32_t>(_recordBuffer[bufferIndex][i]));
    if (value > peak) peak = static_cast<uint16_t>(value);
  }
  _recordLevel = peak;
  _uiDirty = true;
  if ((_recordDataBytes & 0x1FFFF) == 0 && !flushRecordingFile(_recordFile))
    return false;
  return true;
}

void Tab5AudioEngine::finishRecording(bool keepFile) {
  const bool saveRequested = keepFile;
  bool writeFailed = keepFile && !_recordFile;
  int writeError = EIO;
  const uint32_t waitStarted = millis();
  while (M5.Mic.isRecording() && millis() - waitStarted < 500) delay(1);
  M5.Mic.end();
  if (keepFile && _recordFile) {
    size_t completed = (_recordQueueIndex + 3 - _recordPendingCount) % 3;
    for (uint8_t i = 0; i < _recordPendingCount; ++i) {
      if (!writeRecordChunk(completed)) {
        writeFailed = true;
        if (errno) writeError = errno;
        keepFile = false;
        break;
      }
      completed = (completed + 1) % 3;
    }
  }
  _recordPendingCount = 0;
  if (_recordFile) {
    if (keepFile && !writeWavHeader(_recordFile, _recordDataBytes)) {
      writeFailed = true;
      if (errno) writeError = errno;
    }
    if (!flushRecordingFile(_recordFile) && saveRequested) {
      writeFailed = true;
      writeError = errno;
    }
    errno = 0;
    _recordFile.close();
    const int closeError = errno;
    if (closeError && saveRequested) { writeFailed = true; writeError = closeError; }
  }
  if (writeFailed) {
    keepFile = false;
    tab5StorageFault("finalisation enregistrement", _recordPath.c_str(), writeError);
  }
  heap_caps_free(_recordBuffer);
  _recordBuffer = nullptr;
  // A failed user-requested save leaves the already written partial file for
  // recovery. An explicit discard retains its existing removal behavior.
  if (!keepFile && !saveRequested && _recordPath.length())
    tab5FS.remove(_recordPath.c_str());
  if (keepFile) _lastRecordingPath = _recordPath;
  _recordPath = "";
  _mode = Tab5AudioMode::Idle;
  _recordLevel = 0;
  Tab5AudioDevice.resume();
  if (keepFile) {
    _currentLocalPath = _lastRecordingPath;
    reloadLibrary();
    setStatus("Enregistrement sauvegardé");
    setNowPlaying(baseName(_lastRecordingPath));
  } else {
    setStatus(writeFailed ? "Erreur d’écriture sur la carte SD" : "Enregistrement abandonné");
    setNowPlaying("");
  }
}

bool Tab5AudioEngine::execStopRecording() {
  if (_mode != Tab5AudioMode::Recording) return false;
  finishRecording(true);
  return true;
}

void Tab5AudioEngine::setVolume(uint8_t value) {
  if (_output) _output->setMasterVolume(value);
  StateGuard guard(*this);
  if (!guard) return;
  _volume = value;
  Tab5AudioDevice.setVolume(value);
  Tab5MediaSession::volume(value, millis());
  // The UI may refresh immediately after a slider event, before the worker
  // publishes its next snapshot. Do not feed its previous value back to UI.
  if (_snapshotMutex && xSemaphoreTake(_snapshotMutex, portMAX_DELAY) == pdTRUE) {
    _snapshot.volume = value;
    xSemaphoreGive(_snapshotMutex);
  }
  _uiDirty = true;
}

bool Tab5AudioEngine::playing() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::playing);
}

bool Tab5AudioEngine::wifiConnected() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::wifiConnected);
}

int Tab5AudioEngine::wifiRssi() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::wifiRssi);
}

uint32_t Tab5AudioEngine::recordingSeconds() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::recordingSeconds);
}

uint16_t Tab5AudioEngine::playbackProgressPermille() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::playbackProgressPermille);
}

uint32_t Tab5AudioEngine::playbackElapsedSeconds() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::playbackElapsedSeconds);
}

uint32_t Tab5AudioEngine::playbackEstimatedSeconds() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::playbackEstimatedSeconds);
}

bool Tab5AudioEngine::takeUiDirty() {
  if (!_snapshotMutex ||
      xSemaphoreTake(_snapshotMutex, portMAX_DELAY) != pdTRUE) return false;
  const bool value = _snapshotDirty;
  _snapshotDirty = false;
  xSemaphoreGive(_snapshotMutex);
  return value;
}

uint8_t Tab5AudioEngine::volume() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::volume);
}

Tab5AudioMode Tab5AudioEngine::mode() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::mode);
}

bool Tab5AudioEngine::paused() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::paused);
}

bool Tab5AudioEngine::recording() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::recording);
}

uint16_t Tab5AudioEngine::recordingLevel() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::recordingLevel);
}

String Tab5AudioEngine::nowPlaying() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::nowPlaying);
}

String Tab5AudioEngine::status() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::status);
}

String Tab5AudioEngine::lastRecordingPath() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::lastRecordingPath);
}

String Tab5AudioEngine::currentPath() const {
  return audioSnapshotValue(_snapshotMutex, _snapshot, &Tab5AudioSnapshot::currentPath);
}

void Tab5AudioEngine::setStatus(const String& value) {
  if (_status == value) return;
  _status = value;
  _uiDirty = true;
  Tab5DebugSerial.printf("[Audio] %s\n", value.c_str());
}

void Tab5AudioEngine::setNowPlaying(const String& value) {
  if (_nowPlaying == value) return;
  _nowPlaying = value;
  _uiDirty = true;
}

// -----------------------------------------------------------------------------
// API publique appelée depuis l'interface (v0.3.13).
//
// Chaque action se contente de déposer une commande et rend la main
// immédiatement. Auparavant ces fonctions prenaient le mutex global puis
// exécutaient sur place l'ouverture du flux : une poignée de main TLS suivie
// du préchargement figeait LVGL plusieurs secondes. Ces opérations réseau
// restent dans le worker ; le catalogue possède un cycle distinct.
// -----------------------------------------------------------------------------

bool Tab5AudioEngine::playTrack(size_t index, uint32_t revision) {
  if(!revision)revision=trackCatalogueRevision();
  if(!revision || revision!=trackCatalogueRevision())return false;
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::PlayTrack;
  command.index = static_cast<uint32_t>(index);
  command.catalogueRevision=revision;
  return postCommand(command);
}

bool Tab5AudioEngine::playPath(const String& path) {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::PlayPath;
  if (path.length() >= sizeof(command.text)) return false;
  strncpy(command.text, path.c_str(), sizeof(command.text) - 1);
  return postCommand(command);
}

bool Tab5AudioEngine::seekPermille(uint16_t permille) {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::SeekPermille;
  command.index = permille > 1000 ? 1000 : permille;
  return postCommand(command);
}

bool Tab5AudioEngine::playStation(size_t index, uint32_t revision) {
  if(!revision)revision=stationCatalogueRevision();
  if(!revision || revision!=stationCatalogueRevision())return false;
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::PlayStation;
  command.index = static_cast<uint32_t>(index);
  command.catalogueRevision=revision;
  return postCommand(command);
}

void Tab5AudioEngine::previous() {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::Previous;
  postCommand(command);
}

void Tab5AudioEngine::next() {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::Next;
  postCommand(command);
}

void Tab5AudioEngine::togglePause() {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::TogglePause;
  postCommand(command);
}

void Tab5AudioEngine::stop() {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::Stop;
  postCommand(command);
}

bool Tab5AudioEngine::startRecording() {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::StartRecording;
  return postCommand(command);
}

bool Tab5AudioEngine::stopRecording() {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::StopRecording;
  return postCommand(command);
}

bool Tab5AudioEngine::startWifiScan() {
  // Serialize publication with Stop's queue reset; duplicate taps share the
  // pending scan rather than inserting another command behind it.
  StateGuard guard(*this);
  if (!guard) return false;
  if (_wifiScanQueued || _wifiScanRunning) return true;
  _wifiScanQueued = true;
  setWifiScanStatus("Recherche en cours…", true);
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::WifiScan;
  if (postCommand(command)) return true;
  _wifiScanQueued = false;
  setWifiScanStatus("Recherche Wi-Fi impossible", true);
  return false;
}

bool Tab5AudioEngine::connectWifi(const String& ssid, const String& password,
                                  bool remember) {
  if (!ssid.length()) return false;
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::WifiConnect;
  command.flag = remember;
  if (ssid.length() >= sizeof(command.text)) return false;
  if (password.length() >= sizeof(command.secret)) return false;
  strncpy(command.text, ssid.c_str(), sizeof(command.text) - 1);
  strncpy(command.secret, password.c_str(), sizeof(command.secret) - 1);
  return postCommand(command);
}

bool Tab5AudioEngine::setWifiEnabled(bool enabled) {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::WifiEnable;
  command.flag = enabled;
  return postCommand(command);
}

void Tab5AudioEngine::forgetWifi() {
  Tab5AudioCommand command;
  command.type = Tab5AudioCommandType::WifiForget;
  postCommand(command);
}

bool Tab5AudioEngine::storageIdle() const {
  // Internet radio has no SD handle and may keep playing during recovery.
  return !_localSource && !_recordFile && _mode != Tab5AudioMode::Recording;
}

void Tab5AudioEngine::setStorageAvailable(bool available) {
  // Recovery excludes the worker, including queued opens. Do not stop a radio
  // or replace its current station while bringing an unrelated SD back online.
  if (!storageIdle()) return;
  Tab5CatalogueChange change(_trackCatalogueRevision);
  _sdAvailable = available;
  _libraryLoaded = false;
  _stationsLoaded = false;
  std::vector<String>().swap(_tracks);
  _trackIndex = 0;
  if (_mode == Tab5AudioMode::Idle)
    setStatus(available ? "Carte SD reconnectée" : "Carte SD indisponible");
  _uiDirty = true;
}
