#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include "Tab5NetworkGate.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

struct String : std::string {
  using std::string::string;
  using std::string::operator=;
  String() = default;
  String(const std::string& value) : std::string(value) {}
  explicit String(int value) : std::string(std::to_string(value)) {}
};
enum { WIFI_OFF, WIFI_STA, WIFI_AP };
enum { WL_DISCONNECTED = 6, WL_CONNECTED = 3, WIFI_SCAN_RUNNING = -1, WIFI_SCAN_FAILED = -2, WIFI_AUTH_OPEN = 0 };
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
namespace Tab5Connection { constexpr uint8_t MaxRadioRetries = 3; }
struct Tab5WifiNetwork { String ssid; int32_t rssi; bool secured; };
struct TestWifi {
  int modeValue = WIFI_OFF, statusValue = WL_DISCONNECTED;
  int scanStartResult = WIFI_SCAN_RUNNING, scanResult = WIFI_SCAN_RUNNING;
  unsigned begins = 0, scans = 0, disconnects = 0, apStarts = 0, apStops = 0;
  bool allowMode = true, allowAp = true, autoReconnect = false;
  bool allowDisconnect = true, allowApStop = true;
  bool beginProtected = false, disconnectProtected = false, apProtected = false;
  String ssid, password;
  std::vector<Tab5WifiNetwork> records;
  bool mode(int value) { if (!allowMode) return false; modeValue = value; return true; }
  int getMode() const { return modeValue; }
  int status() const { return statusValue; }
  bool setSleep(bool) { return true; }
  void setAutoReconnect(bool value) { autoReconnect = value; }
  void scanDelete() {}
  int16_t scanNetworks(bool, bool) { ++scans; return static_cast<int16_t>(scanStartResult); }
  int16_t scanComplete() const { return static_cast<int16_t>(scanResult); }
  String SSID(int index = -1) const { return index < 0 ? ssid : records.at(index).ssid; }
  int32_t RSSI(int index) const { return records.at(index).rssi; }
  int encryptionType(int index) const { return records.at(index).secured ? 1 : WIFI_AUTH_OPEN; }
  void begin(const char* name, const char* secret) {
    ++begins; ssid = name; password = secret ? secret : "";
    beginProtected = !tab5NetworkEnabled() && tab5NetworkBusy();
  }
  bool disconnect(bool off, bool) {
    ++disconnects;
    if (!allowDisconnect) return false;
    statusValue = WL_DISCONNECTED;
    if (off) { modeValue = WIFI_OFF; disconnectProtected = !tab5NetworkEnabled() && tab5NetworkBusy(); }
    return true;
  }
  bool softAP(const char*, const char*, int, int, int) {
    ++apStarts; modeValue = WIFI_AP;
    apProtected = !tab5NetworkEnabled() && tab5NetworkBusy();
    return allowAp;
  }
  bool softAPdisconnect(bool off) { ++apStops; if (!allowApStop) return false; if (off) modeValue = WIFI_OFF; return true; }
};
extern TestWifi WiFi;
extern bool guardAvailable;
extern std::atomic<bool> busy, enabled, ready;
extern std::atomic<const char*> lastError;
extern std::atomic<uint32_t> code;
extern unsigned freedShareAllocations;
inline bool tab5FileShareBusy() { return busy.load(); }
struct Context { char* page = nullptr; };
inline void heap_caps_free(void*) { ++freedShareAllocations; }
inline int close(int) { return 0; }
inline void vTaskDelay(unsigned ticks) { delay(ticks); }
enum class Tab5AudioMode { Idle, Radio, Recording };
enum class Tab5AudioCommandType { WifiScan };
struct Tab5AudioCommand { Tab5AudioCommandType type; };

class Tab5AudioEngine {
 public:
  class StateGuard {
   public:
    explicit StateGuard(const Tab5AudioEngine&) {}
    explicit operator bool() const { return guardAvailable; }
  };
  Tab5AudioMode _mode = Tab5AudioMode::Idle;
  bool _wifiEnabled = false, _wifiPending = false, _wifiScanRunning = false;
  bool _saveWifiOnConnect = false, _playRadioAfterWifi = false, _radioReconnectPending = false;
  bool _wifiSleeping = false, _paused = false, _uiDirty = false;
  uint8_t _radioRetryCount = 0;
  uint32_t _wifiStartedAt = 0, _wifiScanRevision = 0, _radioReconnectAt = 0;
  size_t _stationIndex = 0;
  void* _streamSource = nullptr;
  String _wifiSsid, _wifiPassword, _wifiScanStatus, _status;
  std::vector<Tab5WifiNetwork> _wifiNetworks;
  bool queueAccept = true, queued = false;
  unsigned saves = 0;
  void setStatus(const String& value) { _status = value; _uiDirty = true; }
  bool saveWifiCredentials() { ++saves; return true; }
  void stopPlayback(bool) { _streamSource = nullptr; }
  bool startRadioNow(size_t) { return true; }
  bool postCommand(const Tab5AudioCommand&) { queued = queueAccept; return queueAccept; }
  bool startWifiScan();
  bool execStartWifiScan();
  void serviceWifiScan();
  bool execConnectWifi(const String&, const String&, bool);
  bool execSetWifiEnabled(bool);
  void serviceWifiConnection();
  size_t wifiNetworkCount() const;
  bool wifiNetwork(size_t, Tab5WifiNetwork&) const;
  bool wifiScanRunning() const;
  uint32_t wifiScanRevision() const;
  String wifiScanStatus() const;
  void setWifiScanStatus(const char*, bool = false);
};
extern Tab5AudioEngine Tab5Audio;

struct lv_obj_t { std::vector<String> rows; };
struct lv_event_t {};
enum { LV_STATE_PRESSED, LV_EVENT_CLICKED };
#define LV_SYMBOL_WIFI "wifi"
#define LV_SYMBOL_OK "ok"
extern lv_obj_t overlay, networkList, other;
extern lv_obj_t *wifiOverlay, *wifiNetworkList, *wifiSelectedLabel, *wifiPasswordInput;
extern uint32_t shownWifiScanRevision;
extern String wifiSelectedSsid;
extern bool wifiSelectedSecured;
inline const char* T(const char* text) { return text; }
inline void lv_obj_clean(lv_obj_t* object) { object->rows.clear(); }
inline void lv_list_add_text(lv_obj_t* object, const char* text) { object->rows.emplace_back(text); }
inline lv_obj_t* lv_list_add_button(lv_obj_t* object, const char*, const char* text) {
  object->rows.emplace_back(text); return &other;
}
inline int C_SURFACE() { return 0; }
inline int C_ACTIVE() { return 0; }
inline int C_TEXT() { return 0; }
inline void lv_obj_set_height(lv_obj_t*, int) {}
inline void lv_obj_set_style_bg_color(lv_obj_t*, int, int) {}
inline void lv_obj_set_style_text_color(lv_obj_t*, int, int) {}
inline void stopAudioRowMarquee(lv_obj_t*, int) {}
inline void* lv_event_get_user_data(lv_event_t*) { return nullptr; }
inline void lv_label_set_text(lv_obj_t*, const char*) {}
inline void lv_textarea_set_text(lv_obj_t*, const char*) {}
inline void lv_obj_add_event_cb(lv_obj_t*, void (*)(lv_event_t*), int, void*) {}
