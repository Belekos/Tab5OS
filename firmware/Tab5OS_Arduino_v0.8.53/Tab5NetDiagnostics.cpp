#include "Tab5Serial.h"
#include "Tab5NetDiagnostics.h"
#include "Tab5Audio.h"

#include <WiFi.h>
#include <esp_attr.h>
#include <sdkconfig.h>

#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
#include <esp32-hal-hosted.h>
#endif

namespace {

static constexpr uint32_t NET_DIAG_MAGIC = 0x54354E44U;  // "T5ND"
static constexpr uint32_t NET_DIAG_MAP = 1U << 0;
static constexpr uint32_t NET_DIAG_RADIO_HTTP = 1U << 1;
static constexpr uint32_t NET_DIAG_RADIO_BUFFER = 1U << 2;
static constexpr uint32_t NET_DIAG_RADIO_DECODER = 1U << 3;
static constexpr uint32_t NET_DIAG_RADIO_PLAYING = 1U << 4;
static constexpr uint32_t NET_DIAG_RADIO_MASK =
    NET_DIAG_RADIO_HTTP | NET_DIAG_RADIO_BUFFER |
    NET_DIAG_RADIO_DECODER | NET_DIAG_RADIO_PLAYING;
static constexpr uint32_t NET_DIAG_VIDEO_HTTP = 1U << 5;
static constexpr uint32_t NET_DIAG_VIDEO_STREAM = 1U << 6;
static constexpr uint32_t NET_DIAG_VIDEO_MASK =
    NET_DIAG_VIDEO_HTTP | NET_DIAG_VIDEO_STREAM;
static constexpr uint32_t NET_DIAG_KNOWN_MASK =
    NET_DIAG_MAP | NET_DIAG_RADIO_MASK | NET_DIAG_VIDEO_MASK;

struct Tab5RtcNetworkState {
  uint32_t magic;
  uint32_t flags;
};

RTC_NOINIT_ATTR volatile Tab5RtcNetworkState rtcNetworkState;
bool wifiReportPrinted = false;

uint32_t loadFlags() {
  return __atomic_load_n(&rtcNetworkState.flags, __ATOMIC_RELAXED);
}

void storeFlags(uint32_t flags) {
  __atomic_store_n(&rtcNetworkState.flags,
                   flags & NET_DIAG_KNOWN_MASK, __ATOMIC_RELEASE);
}

void replaceFlags(uint32_t clearMask, uint32_t setMask) {
  uint32_t expected = loadFlags();
  uint32_t desired = 0;
  do {
    desired = (expected & ~clearMask) | setMask;
  } while (!__atomic_compare_exchange_n(
      &rtcNetworkState.flags, &expected, desired, false,
      __ATOMIC_RELEASE, __ATOMIC_RELAXED));
}

const char* radioStageName(uint32_t flags) {
  if (flags & NET_DIAG_RADIO_PLAYING) return "lecture WebRadio";
  if (flags & NET_DIAG_RADIO_DECODER) return "demarrage decodeur WebRadio";
  if (flags & NET_DIAG_RADIO_BUFFER) return "prechargement WebRadio";
  if (flags & NET_DIAG_RADIO_HTTP) return "connexion HTTP/TLS WebRadio";
  return nullptr;
}

const char* videoStageName(uint32_t flags) {
  if (flags & NET_DIAG_VIDEO_STREAM) return "lecture flux video";
  if (flags & NET_DIAG_VIDEO_HTTP) return "connexion HTTP/TLS video";
  return nullptr;
}

uint32_t radioStageFlag(Tab5RadioNetworkStage stage) {
  switch (stage) {
    case Tab5RadioNetworkStage::HttpOpen: return NET_DIAG_RADIO_HTTP;
    case Tab5RadioNetworkStage::Buffering: return NET_DIAG_RADIO_BUFFER;
    case Tab5RadioNetworkStage::DecoderStart: return NET_DIAG_RADIO_DECODER;
    case Tab5RadioNetworkStage::Playing: return NET_DIAG_RADIO_PLAYING;
    default: return 0;
  }
}

}  // namespace

void tab5NetDiagnosticsBegin() {
  uint32_t previous = 0;
  if (rtcNetworkState.magic == NET_DIAG_MAGIC)
    previous = loadFlags() & NET_DIAG_KNOWN_MASK;

  rtcNetworkState.magic = NET_DIAG_MAGIC;
  storeFlags(0);

  if (!previous) {
    Tab5DebugSerial.println("[NET][RESET] aucune operation reseau interrompue");
    return;
  }

  Tab5DebugSerial.print("[NET][RESET] operation interrompue : ");
  bool separator = false;
  if (previous & NET_DIAG_MAP) {
    Tab5DebugSerial.print("requete Cartes");
    separator = true;
  }
  const char* radio = radioStageName(previous);
  if (radio) {
    if (separator) Tab5DebugSerial.print(" + ");
    Tab5DebugSerial.print(radio);
    separator = true;
  }
  const char* video = videoStageName(previous);
  if (video) {
    if (separator) Tab5DebugSerial.print(" + ");
    Tab5DebugSerial.print(video);
  }
  Tab5DebugSerial.println();
  Tab5DebugSerial.flush();
}

void tab5NetDiagnosticsService() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (__atomic_exchange_n(&wifiReportPrinted, true, __ATOMIC_ACQ_REL)) return;

  const String local = WiFi.localIP().toString();
  const String gateway = WiFi.gatewayIP().toString();
  const String dns = WiFi.dnsIP().toString();
  const int rssi = Tab5Audio.wifiRssi();
  char signalText[20];
  if (rssi < 0) snprintf(signalText, sizeof(signalText), "%d dBm", rssi);
  else snprintf(signalText, sizeof(signalText), "n/a");
  Tab5DebugSerial.printf("[WIFI] IPv4=%s | passerelle=%s | DNS=%s | RSSI(cache)=%s\n",
                local.c_str(), gateway.c_str(), dns.c_str(), signalText);

#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
  if (!hostedIsInitialized()) {
    Tab5DebugSerial.println("[HOSTED] transport P4-C6 non initialise");
    return;
  }
  uint32_t hostMajor = 0, hostMinor = 0, hostPatch = 0;
  uint32_t slaveMajor = 0, slaveMinor = 0, slavePatch = 0;
  hostedGetHostVersion(&hostMajor, &hostMinor, &hostPatch);
  hostedGetSlaveVersion(&slaveMajor, &slaveMinor, &slavePatch);
  const bool slaveKnown = slaveMajor || slaveMinor || slavePatch;
  const bool versionsMatch = slaveKnown && hostMajor == slaveMajor &&
      hostMinor == slaveMinor && hostPatch == slavePatch;
  Tab5DebugSerial.printf("[HOSTED] hote %lu.%lu.%lu | C6 %lu.%lu.%lu | %s\n",
                static_cast<unsigned long>(hostMajor),
                static_cast<unsigned long>(hostMinor),
                static_cast<unsigned long>(hostPatch),
                static_cast<unsigned long>(slaveMajor),
                static_cast<unsigned long>(slaveMinor),
                static_cast<unsigned long>(slavePatch),
                !slaveKnown ? "version C6 illisible"
                            : (versionsMatch ? "versions identiques"
                                             : "versions differentes"));
#else
  Tab5DebugSerial.println("[HOSTED] transport Wi-Fi deporte non active dans ce build");
#endif
  Tab5DebugSerial.flush();
}

void tab5NetDiagnosticsMapBegin() {
  replaceFlags(0, NET_DIAG_MAP);
}

void tab5NetDiagnosticsMapEnd() {
  replaceFlags(NET_DIAG_MAP, 0);
}

void tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage stage) {
  replaceFlags(NET_DIAG_RADIO_MASK, radioStageFlag(stage));
}

void tab5NetDiagnosticsRadioEnd() {
  replaceFlags(NET_DIAG_RADIO_MASK, 0);
}

void tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage stage) {
  const uint32_t flag = stage == Tab5VideoNetworkStage::HttpOpen
      ? NET_DIAG_VIDEO_HTTP : stage == Tab5VideoNetworkStage::Streaming
      ? NET_DIAG_VIDEO_STREAM : 0;
  replaceFlags(NET_DIAG_VIDEO_MASK, flag);
}

void tab5NetDiagnosticsVideoEnd() {
  replaceFlags(NET_DIAG_VIDEO_MASK, 0);
}
