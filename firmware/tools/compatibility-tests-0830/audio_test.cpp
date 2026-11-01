#include "Tab5Compatibility.h"
#include "M5Unified.h"
#include "esp_hosted.h"
#include <cstdio>
#include <cstdlib>
#include <string>

static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); abort(); } } while(0)
using String = std::string;
constexpr int WIFI_OFF = 0, WIFI_STA = 1, WL_CONNECTED = 3;
constexpr int WIFI_SCAN_RUNNING = -1, WIFI_SCAN_FAILED = -2;
struct FakeWifi {
  bool modeOk = true, disconnectOk = true, sleepOk = true;
  int currentMode = WIFI_OFF, currentStatus = 0, scanResult = WIFI_SCAN_RUNNING;
  unsigned modeCalls = 0, beginCalls = 0, disconnectCalls = 0, scanCalls = 0, deleteCalls = 0;
  String currentSsid, lastPassword;
  bool mode(int value) { ++modeCalls; if (modeOk) currentMode = value; return modeOk; }
  bool setSleep(bool) { return sleepOk; }
  void setAutoReconnect(bool) {}
  void scanDelete() { ++deleteCalls; }
  int16_t scanNetworks(bool async, bool hidden) { CHECK(async && hidden); ++scanCalls; return scanResult; }
  int status() const { return currentStatus; }
  String SSID() const { return currentSsid; }
  int getMode() const { return currentMode; }
  void disconnect(bool off, bool erase) {
    CHECK(!erase); ++disconnectCalls;
    if (disconnectOk) { currentStatus = 0; if (off) currentMode = WIFI_OFF; }
  }
  void begin(const char* ssid, const char* password) {
    ++beginCalls; currentSsid = ssid; lastPassword = password ? password : "";
  }
} WiFi;
static bool fileShareBusy = false, networkBusy = false, mediaPending = false;
static bool networkEnabled = false, transitionOk = true, guardOk = true;
static unsigned transitions = 0;
bool tab5FileShareBusy() { return fileShareBusy; }
bool tab5NetworkBusy() { return networkBusy; }
bool tab5NetworkMediaPending() { return mediaPending; }
void tab5NetworkSetEnabled(bool value) { networkEnabled = value; }
uint32_t millis() { return 1234; }
struct Tab5NetworkTransition {
  Tab5NetworkTransition(const char*, unsigned timeout) { CHECK(timeout == 20000); ++transitions; }
  explicit operator bool() const { return transitionOk; }
};
enum class Tab5AudioMode { Idle, Recording, Radio };
class Tab5AudioEngine {
 public:
  struct StateGuard {
    explicit StateGuard(Tab5AudioEngine&) {}
    explicit operator bool() const { return guardOk; }
  };
  bool _wifiScanQueued = true, _wifiScanRunning = false, _wifiEnabled = false;
  bool _wifiPending = false, _wifiSleeping = true, _saveWifiOnConnect = false;
  bool _playRadioAfterWifi = false, _radioReconnectPending = false;
  unsigned _wifiScanRevision = 0, _wifiStartedAt = 0, saved = 0, stopped = 0;
  Tab5AudioMode _mode = Tab5AudioMode::Idle;
  void* _streamSource = nullptr;
  String _wifiSsid, _wifiPassword, status, scanStatus;
  void setStatus(const String& text) { status = text; }
  void setWifiScanStatus(const String& text, bool = false) { scanStatus = text; }
  void saveWifiCredentials() { ++saved; }
  void stopPlayback(bool) { ++stopped; _mode = Tab5AudioMode::Idle; _streamSource = nullptr; }
  bool execStartWifiScan();
  bool execConnectWifi(const String&, const String&, bool);
  bool execSetWifiEnabled(bool);
};
// Generated each run from exact production method source, not a rewritten model.
#include "audio_methods.inc"

FakeM5 M5;
static int monitorFailure = 0;
static esp_event_handler_t handler = nullptr;
lgfx::i2c::Result lgfx::i2c::transactionWriteRead(int,int,const uint8_t*,size_t,uint8_t*,size_t,int) {
  CHECK(false); return {false}; // Network paths must not touch the display bus.
}
int esp_event_loop_create_default() { return monitorFailure == 1 ? 257 : ESP_OK; }
int esp_event_handler_instance_register(esp_event_base_t,int32_t,esp_event_handler_t callback,
    void*,esp_event_handler_instance_t* result) {
  if (monitorFailure == 2) return 257;
  handler = callback; *result = reinterpret_cast<void*>(1); return ESP_OK;
}
// No version RPC implementation: the full adapter and real three methods link
// and run without the firmware-version query, including failed monitoring.
static Tab5AudioEngine fresh() {
  WiFi = {}; fileShareBusy = networkBusy = mediaPending = networkEnabled = false;
  transitionOk = guardOk = true; transitions = 0;
  return {};
}
int main(int argc, char** argv) {
  if (argc > 1) monitorFailure = std::atoi(argv[1]);
  Tab5Compatibility::beforeWifiStart();
  CHECK(Tab5Compatibility::snapshot().eventMonitoring == (monitorFailure == 0));
  // A diagnostic transport status cannot pre-empt a successful driver start.
  // Actual driver failure is separately verified below.
  const int events[] = {ESP_HOSTED_EVENT_CP_INIT, ESP_HOSTED_EVENT_TRANSPORT_UP,
                        ESP_HOSTED_EVENT_TRANSPORT_DOWN, ESP_HOSTED_EVENT_TRANSPORT_FAILURE};
  for (const int event : events) {
    if (handler) { esp_hosted_event_init_t data{7}; handler(nullptr, ESP_HOSTED_EVENT, event, &data); }
    auto e = fresh();
    CHECK(e.execStartWifiScan());
    CHECK(e._wifiEnabled && networkEnabled && e._wifiScanRunning && !e._wifiScanQueued);
    CHECK(WiFi.modeCalls == 1 && WiFi.scanCalls == 1 && e._wifiScanRevision == 1);
    CHECK(!e._wifiSleeping && WiFi.beginCalls == 0);
    e = fresh();
    CHECK(e.execConnectWifi("test", "secret", true));
    CHECK(e._wifiEnabled && networkEnabled && e._wifiPending && e._saveWifiOnConnect);
    CHECK(WiFi.modeCalls == 1 && WiFi.beginCalls == 1 && WiFi.currentSsid == "test");
    CHECK(WiFi.lastPassword == "secret" && e._wifiStartedAt == 1234);
    e = fresh();
    CHECK(e.execSetWifiEnabled(true));
    CHECK(e._wifiEnabled && networkEnabled && WiFi.modeCalls == 1 && WiFi.beginCalls == 0);
  }

  // The real WiFi.mode() refusal still rejects all three entry points.
  auto e = fresh(); WiFi.modeOk = false;
  CHECK(!e.execStartWifiScan());
  CHECK(!e._wifiEnabled && !networkEnabled && !e._wifiScanRunning && WiFi.scanCalls == 0);
  e = fresh(); WiFi.modeOk = false;
  CHECK(!e.execConnectWifi("test", "secret", false));
  CHECK(!e._wifiEnabled && !networkEnabled && !e._wifiPending && WiFi.beginCalls == 0);
  e = fresh(); WiFi.modeOk = false;
  CHECK(!e.execSetWifiEnabled(true));
  CHECK(!e._wifiEnabled && !networkEnabled && WiFi.beginCalls == 0);
  e = fresh(); WiFi.scanResult = WIFI_SCAN_FAILED;
  CHECK(!e.execStartWifiScan());
  CHECK(!e._wifiScanRunning && WiFi.deleteCalls == 2);

  // File-sharing exclusion applies to scan, connection, enable and disable.
  e = fresh(); fileShareBusy = true;
  CHECK(!e.execStartWifiScan() && !e.execConnectWifi("test", "", false));
  CHECK(!e.execSetWifiEnabled(true) && !e.execSetWifiEnabled(false));
  CHECK(WiFi.modeCalls == 0 && WiFi.beginCalls == 0 && transitions == 0);
  for (unsigned busy = 0; busy < 4; ++busy) {
    e = fresh();
    if (busy == 0) e._mode = Tab5AudioMode::Recording;
    if (busy == 1) e._wifiPending = true;
    if (busy == 2) networkBusy = true;
    if (busy == 3) mediaPending = true;
    CHECK(!e.execStartWifiScan());
    CHECK(WiFi.modeCalls == 0 && !e._wifiScanRunning);
  }
  e = fresh(); transitionOk = false; e._wifiEnabled = true; networkEnabled = true;
  CHECK(!e.execConnectWifi("test", "", false));
  CHECK(networkEnabled && e._wifiEnabled && WiFi.modeCalls == 0 && WiFi.beginCalls == 0);
  e = fresh(); transitionOk = false; e._wifiEnabled = true; networkEnabled = true;
  CHECK(!e.execSetWifiEnabled(false));
  CHECK(networkEnabled && e._wifiEnabled && WiFi.disconnectCalls == 0);

  // Saved credentials and in-flight/already connected operations retain behavior.
  e = fresh();
  CHECK(!e.execConnectWifi("", "", false) && WiFi.modeCalls == 0);
  e = fresh(); e._wifiSsid = "saved"; e._wifiPassword = "password";
  CHECK(e.execSetWifiEnabled(true));
  CHECK(WiFi.currentSsid == "saved" && WiFi.lastPassword == "password" && e._wifiPending);
  e = fresh(); e._wifiSsid = "saved"; e._wifiPassword = "password";
  CHECK(e.execConnectWifi("saved", "", true));
  CHECK(WiFi.lastPassword == "password");
  e = fresh(); e._wifiPending = true; e._wifiSsid = "pending"; e._wifiPassword = "password";
  CHECK(e.execConnectWifi("pending", "", true));
  CHECK(e._saveWifiOnConnect && WiFi.modeCalls == 0 && WiFi.beginCalls == 0);
  CHECK(e.execSetWifiEnabled(true) && e._wifiEnabled && networkEnabled);
  e = fresh(); WiFi.currentStatus = WL_CONNECTED; WiFi.currentSsid = "connected";
  CHECK(e.execConnectWifi("connected", "", true));
  CHECK(e.saved == 1 && WiFi.modeCalls == 0);
  CHECK(e.execSetWifiEnabled(true) && e._wifiEnabled && networkEnabled && WiFi.modeCalls == 0);

  // Explicit off still stops radio, cancels a scan and shuts down the driver.
  e = fresh(); e._wifiEnabled = e._wifiPending = e._wifiScanRunning = true;
  e._saveWifiOnConnect = e._playRadioAfterWifi = e._radioReconnectPending = true;
  e._mode = Tab5AudioMode::Radio; WiFi.currentMode = WIFI_STA;
  CHECK(e.execSetWifiEnabled(false));
  CHECK(!e._wifiEnabled && !networkEnabled && !e._wifiPending && !e._wifiScanRunning);
  CHECK(!e._saveWifiOnConnect && !e._playRadioAfterWifi && !e._radioReconnectPending);
  CHECK(WiFi.currentMode == WIFI_OFF && WiFi.disconnectCalls == 1 && WiFi.deleteCalls == 1 && e.stopped == 1);
  e = fresh(); e._wifiEnabled = true; WiFi.currentMode = WIFI_STA; WiFi.disconnectOk = false;
  CHECK(!e.execSetWifiEnabled(false));
  CHECK(e._wifiEnabled && networkEnabled && WiFi.currentMode == WIFI_STA);
  printf("PASS %u audio checks; exact_production_methods=3; monitoring_failure=%d; version_RPC=absent_at_link\n", checks, monitorFailure);
}
