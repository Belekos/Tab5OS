#include "Platform.h"
#include <stdio.h>

uint32_t testMilliseconds = 0;
unsigned testTakes = 0, testGives = 0;
void (*testDelayHook)() = nullptr;
void (*testTakeHook)() = nullptr;
TestSerial Serial;
TestWifi WiFi;
bool guardAvailable = true;
std::atomic<bool> busy{false}, enabled{false}, ready{false};
std::atomic<const char*> lastError{""};
std::atomic<uint32_t> code{0};
unsigned freedShareAllocations = 0;
Tab5AudioEngine Tab5Audio;
lv_obj_t overlay, networkList, other;
lv_obj_t *wifiOverlay = &overlay, *wifiNetworkList = &networkList, *wifiSelectedLabel = &other, *wifiPasswordInput = &other;
lv_obj_t scanButton,connectButton,forgetButton;
lv_obj_t *wifiScanButton=&scanButton,*wifiConnectButton=&connectButton,*wifiForgetButton=&forgetButton;
uint32_t shownWifiScanRevision = 0;
String wifiSelectedSsid;
bool wifiSelectedSecured = false;

#include "production-flow.inc"

static void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
static void reset() {
  WiFi = TestWifi{}; Tab5Audio.~Tab5AudioEngine(); new (&Tab5Audio) Tab5AudioEngine{};
  busy = false; enabled = false; ready = false; lastError = ""; code = 0;
  freedShareAllocations = 0; guardAvailable = true;
  testDelayHook = nullptr; testTakeHook = nullptr;
  tab5NetworkSetEnabled(false); tab5NetworkSetBackgroundRetained(false);
  tab5NetworkSetMediaPending(false);
  wifiSelectedSsid="";wifiSelectedSecured=false;other.text="";
  Tab5Compatibility::allowed=true;Tab5Compatibility::beforeCalls=Tab5Compatibility::afterCalls=0;
}
static std::unique_ptr<Tab5NetworkLease> activeClient;
static void releaseClient() { activeClient.reset(); }
static void disableNetwork() { tab5NetworkSetEnabled(false); }
static bool cancelled(void*) { return true; }
static bool backgroundYieldObserved = false;
static void releaseBackgroundClient() {
  backgroundYieldObserved = tab5NetworkBackgroundYieldRequested() && !tab5NetworkEnabled();
  tab5NetworkSetBackgroundRetained(false);
}
static void recoverApDriver() {
  check(busy && !ready && !enabled && WiFi.getMode() == WIFI_AP,
        "failed AP cleanup released ownership or still accepts requests");
  check(strcmp(lastError.load(), "Arrêt réseau en attente — réessayez") == 0,
        "AP stop failure not reported while waiting");
  check(!Tab5Audio.execSetWifiEnabled(true), "station changed while AP shutdown was pending");
  check(freedShareAllocations == 2, "retry retained the large sharing context");
  WiFi.allowApStop = true;
}

int main() {
 try {
  tab5NetworkGateBegin(); reset();
#ifdef TAB5_BASELINE
  unsigned regressions = 0;
  if (!Tab5Audio.execConnectWifi("test-network", "test-password", true) && WiFi.begins == 0) {
    ++regressions; puts("EXPECTED-FAIL: connection rejected before WiFi.begin");
  }
  reset(); Tab5Audio._wifiEnabled = true; tab5NetworkSetEnabled(true); WiFi.modeValue = WIFI_STA;
  if (!Tab5Audio.execSetWifiEnabled(false) && WiFi.getMode() == WIFI_STA) {
    ++regressions; puts("EXPECTED-FAIL: OFF rejected before driver shutdown");
  }
  reset();
  if (!baselineAccessPointStart() && WiFi.apStarts == 0) {
    ++regressions; puts("EXPECTED-FAIL: direct AP rejected while Wi-Fi is off");
  }
  return regressions == 3 ? 1 : 2;
#else
  refreshWifiPopupActions();
  check(scanButton.enabled&&!connectButton.enabled&&!forgetButton.enabled,"empty Wi-Fi popup must expose only Scan");
  wifiSelectedSsid="protected";wifiSelectedSecured=true;refreshWifiPopupActions();
  check(!connectButton.enabled,"secured network without password must disable Connect");
  other.text="password";refreshWifiPopupActions();check(connectButton.enabled,"entered password must enable Connect");
  other.text="";wifiSelectedSecured=false;refreshWifiPopupActions();check(connectButton.enabled,"open network must permit Connect without password");
  Tab5Audio._wifiSsid="protected";wifiSelectedSecured=true;refreshWifiPopupActions();
  check(connectButton.enabled&&forgetButton.enabled,"saved network permits reuse and Forget");
  wifiSelectedSsid="other";refreshWifiPopupActions();check(!connectButton.enabled&&forgetButton.enabled,"other secured network cannot reuse saved password");
  busy=true;refreshWifiPopupActions();check(!scanButton.enabled&&!connectButton.enabled&&!forgetButton.enabled,"active sharing must disable unavailable Wi-Fi actions");
  reset();Tab5Compatibility::allowed=false;refreshWifiPopupActions();
  check(!scanButton.enabled&&!connectButton.enabled&&!forgetButton.enabled,"unsupported C6 firmware disables Wi-Fi actions");
  check(!Tab5Audio.execStartWifiScan()&&WiFi.scans==0,"unsupported C6 firmware prevents scan driver operation");
  check(!tab5NetworkEnabled()&&!tab5NetworkBusy(),"rejected C6 scan keeps client gate closed and releases transition");
  check(Tab5Compatibility::beforeCalls==1&&Tab5Compatibility::afterCalls==1,"scan checks C6 compatibility around transport start");
  reset();Tab5Compatibility::allowed=false;
  check(!Tab5Audio.execConnectWifi("test-network","test-password",false)&&WiFi.begins==0,"unsupported C6 firmware prevents connection driver operation");
  check(!tab5NetworkEnabled()&&!tab5NetworkBusy(),"rejected C6 connection keeps client gate closed and releases transition");
  reset();Tab5Compatibility::allowed=false;
  check(!Tab5Audio.execSetWifiEnabled(true)&&WiFi.begins==0,"unsupported C6 firmware rejects station enable");
  check(!tab5NetworkEnabled()&&!tab5NetworkBusy(),"rejected C6 station enable keeps client gate closed and releases transition");
  reset();
  check(Tab5Audio.execConnectWifi("test-network", "test-password", true), "connection rejected before WiFi.begin");
  check(WiFi.begins == 1 && WiFi.beginProtected, "WiFi.begin must run under OS control while clients are blocked");
  check(Tab5Audio._wifiPending && tab5NetworkEnabled(), "connection state not published");
  static_assert(!std::is_constructible<Tab5NetworkTransition, const char*, uint32_t>::value,
                "apps must not construct an OS transition");
  WiFi.statusValue = WL_CONNECTED;
  Tab5Audio.serviceWifiConnection();
  check(!Tab5Audio._wifiPending && Tab5Audio.saves == 1, "successful connection must save credentials once");
  check(Tab5Audio.execSetWifiEnabled(false), "cannot disable a connected radio");
  check(!tab5NetworkEnabled() && !Tab5Audio._wifiEnabled && WiFi.getMode() == WIFI_OFF && WiFi.disconnectProtected,
        "radio off must block clients and close the driver under control");
  { Tab5NetworkLease app("application", 100); check(!app, "disabled network admitted an application"); }
  check(Tab5Audio.execSetWifiEnabled(true), "saved network cannot reconnect");
  check(WiFi.begins == 2, "saved reconnect never reached driver");
  testMilliseconds += WIFI_TIMEOUT_MS;
  Tab5Audio.serviceWifiConnection();
  check(!Tab5Audio._wifiPending && !WiFi.autoReconnect && Tab5Audio._status == "Échec de connexion Wi-Fi",
        "wrong password timeout did not release pending state");
  check(Tab5Audio.execStartWifiScan(), "scan rejected after connection timeout");

  reset();
  check(Tab5Audio.startWifiScan() && Tab5Audio.commandQueue.commands.size()==1, "scan request was not queued");
  check(Tab5Audio.startWifiScan() && Tab5Audio.commandQueue.sends==1,"duplicate pending scan must not enqueue another command");
  refreshWifiPopupList(true);
  check(!scanButton.enabled&&!connectButton.enabled&&!forgetButton.enabled,"queued scan disables duplicate or conflicting Wi-Fi actions");
  check(networkList.rows.size() == 1 && networkList.rows[0] == "Recherche en cours…", "queued scan incorrectly says no networks");
  check(Tab5Audio.execStartWifiScan(), "scan cannot turn radio on from off");
  check(!Tab5Audio._wifiScanQueued&&Tab5Audio.wifiScanRunning(),"worker transfers pending scan to active state");
  check(Tab5Audio.startWifiScan()&&Tab5Audio.commandQueue.sends==1,"duplicate active scan must not enqueue another command");
  check(Tab5Audio._wifiEnabled && tab5NetworkEnabled() && WiFi.getMode() == WIFI_STA, "scan left radio and admission inconsistent");
  WiFi.records = {{"weak", -85, true}, {"strong", -30, false}, {"weak", -40, true}, {"", -20, false}};
  WiFi.scanResult = 4;
  Tab5Audio.serviceWifiScan(); refreshWifiPopupList(false);
  check(!Tab5Audio.wifiScanRunning()&&scanButton.enabled,"completed scan releases unavailable controls");
  check(Tab5Audio.wifiNetworkCount() == 2 && networkList.rows.size() == 2, "scan results not published to actual popup flow");
  check(Tab5Audio._wifiNetworks[0].ssid == "strong" && Tab5Audio._wifiNetworks[1].rssi == -40, "scan deduplication/sorting changed");
  check(Tab5Audio.execSetWifiEnabled(false), "off after scan failed"); refreshWifiPopupList(false);
  check(networkList.rows.size() == 1 && networkList.rows[0] == "Wi-Fi désactivé", "off left stale scan results");

  reset(); WiFi.scanStartResult = WIFI_SCAN_FAILED;
  check(!Tab5Audio.execStartWifiScan(), "failed scan accepted"); refreshWifiPopupList(false);
  check(networkList.rows[0] == "Recherche Wi-Fi impossible", "scan failure shown as empty successful result");
  WiFi.scanStartResult = WIFI_SCAN_RUNNING;
  check(Tab5Audio.execStartWifiScan(), "scan cannot retry after immediate failure");
  WiFi.scanResult = 0; Tab5Audio.serviceWifiScan(); refreshWifiPopupList(false);
  check(networkList.rows[0] == "Aucun réseau détecté", "zero results not distinguished from failure");
  check(Tab5Audio.execStartWifiScan(), "second scan cannot start");
  WiFi.scanResult = WIFI_SCAN_FAILED; Tab5Audio.serviceWifiScan(); refreshWifiPopupList(false);
  check(networkList.rows[0] == "Recherche Wi-Fi impossible", "asynchronous scan failure not shown");
  Tab5Audio._wifiPending = true; unsigned before = Tab5Audio.wifiScanRevision();
  check(!Tab5Audio.execStartWifiScan() && Tab5Audio.wifiScanRevision() != before, "deferred scan did not refresh status");
  refreshWifiPopupList(false);
  check(networkList.rows[0] == "Scan différé : connexion ou lecture en cours", "deferred scan shown as empty result");

  reset(); Tab5Audio.commandQueue.accept = false;
  check(!Tab5Audio.startWifiScan(), "rejected queue accepted"); refreshWifiPopupList(false);
  check(!Tab5Audio.wifiScanRunning()&&scanButton.enabled,"rejected queue releases pending state and controls");
  check(networkList.rows[0] == "Recherche Wi-Fi impossible", "queue failure leaves endless scanning message");
  Tab5Audio.commandQueue.accept=true;
  check(Tab5Audio.startWifiScan(),"scan can retry after full queue");
  const Tab5AudioCommand stop{Tab5AudioCommandType::Stop};
  check(Tab5Audio.postCommand(stop),"Stop must replace queued scan");refreshWifiPopupList(false);
  check(!Tab5Audio.wifiScanRunning()&&scanButton.enabled&&networkList.rows[0]=="Arrêté","Stop cancels waiting scan state and status");
  check(Tab5Audio.commandQueue.resets==1&&Tab5Audio.commandQueue.commands.size()==1&&Tab5Audio.commandQueue.commands[0].type==Tab5AudioCommandType::Stop,"Stop queue reset retains no scan command");
  check(Tab5Audio.startWifiScan()&&Tab5Audio.execStartWifiScan(),"scan can restart after Stop");
  check(Tab5Audio.postCommand(stop)&&Tab5Audio.wifiScanRunning(),"media Stop must preserve an already active scan");
  reset();check(Tab5Audio.startWifiScan(),"scan must queue before worker refusal");
  busy=true;check(!Tab5Audio.execStartWifiScan()&&!Tab5Audio.wifiScanRunning(),"worker refusal releases queued and active scan state");
  busy=false;check(Tab5Audio.startWifiScan(),"scan can retry after worker refusal");
  reset();std::vector<std::thread> scanTaps;
  for(unsigned i=0;i<16;++i)scanTaps.emplace_back([]{check(Tab5Audio.startWifiScan(),"concurrent scan tap rejected");});
  for(auto& tap:scanTaps)tap.join();
  check(Tab5Audio.commandQueue.sends==1&&Tab5Audio.wifiScanRunning(),"concurrent scan taps must coalesce into one pending command");
  reset(); check(Tab5Audio.execSetWifiEnabled(true) && WiFi.begins == 0, "no saved SSID must only enable station");
  check(Tab5Audio.execStartWifiScan(), "no-SSID station cannot scan");
  reset(); WiFi.allowMode = false;
  check(!Tab5Audio.execSetWifiEnabled(true) && !tab5NetworkEnabled() && !Tab5Audio._wifiEnabled,
        "failed enable left network enabled");
  check(!Tab5Audio.execStartWifiScan(), "failed driver initialization accepted scan");
  reset(); Tab5Audio._wifiSsid = "saved"; Tab5Audio._wifiPassword = "test-password"; WiFi.allowMode = false;
  check(!Tab5Audio.execSetWifiEnabled(true) && !tab5NetworkEnabled() && !Tab5Audio._wifiEnabled,
        "saved-network activation changed OFF state before failed driver initialization");
  reset(); Tab5Audio._wifiEnabled = true; tab5NetworkSetEnabled(true); WiFi.modeValue = WIFI_STA; WiFi.allowDisconnect = false;
  check(!Tab5Audio.execSetWifiEnabled(false) && tab5NetworkEnabled() && Tab5Audio._wifiEnabled && WiFi.getMode() == WIFI_STA,
        "failed driver stop falsely announced Wi-Fi OFF");
  WiFi.allowDisconnect = true;
  check(Tab5Audio.execSetWifiEnabled(false) && !tab5NetworkEnabled(), "OFF cannot retry after driver failure");
  reset(); guardAvailable = false;
  check(!Tab5Audio.execConnectWifi("test", "", false) && !tab5NetworkEnabled() && !tab5NetworkBusy(),
        "state-lock failure leaked admission or mutex"); guardAvailable = true;

  reset(); tab5NetworkSetEnabled(true);
  activeClient.reset(new Tab5NetworkLease("existing application", 100));
  check(bool(*activeClient), "existing application lease failed");
  testTakeHook = releaseClient;
  check(Tab5Audio.execConnectWifi("test", "", false), "OS transition failed to wait for active client");
  testTakeHook = nullptr;
  check(WiFi.beginProtected, "transition reopened client admission before driver change");
  reset(); tab5NetworkSetBackgroundRetained(true);
  backgroundYieldObserved = false; testDelayHook = releaseBackgroundClient;
  check(Tab5Audio.execConnectWifi("test", "", false), "transition failed to drain retained background client");
  testDelayHook = nullptr;
  check(backgroundYieldObserved && WiFi.beginProtected, "OS transition bypassed background ownership or reopened admission");
  reset(); tab5NetworkSetEnabled(true); Tab5Audio._wifiSsid = "saved";
  {
    Tab5NetworkLease owner("client retained before OFF", 100);
    check(bool(owner), "cannot prepare existing owner before OFF");
    tab5NetworkSetEnabled(false);
    unsigned began = testMilliseconds;
    check(!Tab5Audio.execSetWifiEnabled(true), "saved activation stole retained client's mutex");
    check(testMilliseconds - began >= 20000 && !tab5NetworkEnabled() && !Tab5Audio._wifiEnabled,
          "failed public saved activation changed previous OFF state");
    check(WiFi.begins == 0 && WiFi.getMode() == WIFI_OFF && tab5NetworkBusy(),
          "failed saved activation changed driver or released another owner's mutex");
  }
  reset(); tab5NetworkSetEnabled(true); Tab5Audio._wifiEnabled = true; WiFi.modeValue = WIFI_STA;
  {
    Tab5NetworkLease owner("held application", 100);
    unsigned began = testMilliseconds;
    check(!Tab5Audio.execSetWifiEnabled(false), "busy transition stole the mutex");
    check(testMilliseconds - began >= 20000 && tab5NetworkEnabled() && Tab5Audio._wifiEnabled,
          "transition timeout failed to restore previous enabled state");
    check(WiFi.disconnects == 0 && tab5NetworkBusy(), "timeout modified driver or released another owner's mutex");
  }
  check(!tab5NetworkBusy(), "mutex not released after timed-out transition");
  reset(); tab5NetworkSetEnabled(true); testTakeHook = disableNetwork;
  { Tab5NetworkLease app("disabled during acquisition", 100); check(!app, "racing application bypassed disable"); }
  testTakeHook = nullptr;
  check(!tab5NetworkBusy(), "race rejection leaked mutex");
  tab5NetworkSetEnabled(true);
  { Tab5NetworkLease app("cancelled", 100, cancelled); check(!app, "cancelled application accepted"); }
  check(!tab5NetworkBusy(), "cancellation leaked mutex");
  { Tab5NetworkLease app("re-enabled", 100); check(bool(app), "re-enable did not admit application"); }

  reset(); bool attempted = false;
  check(!Tab5FileShareNetworkTransition::start("Tab5-test", "test-password", attempted), "direct AP cannot start while station is off");
  check(attempted && WiFi.apStarts == 1 && WiFi.apProtected && !tab5NetworkEnabled(), "AP changed client policy or skipped control gate");
  { Tab5NetworkLease app("application while direct sharing", 100); check(!app, "AP incorrectly permits Internet clients"); }
  busy = true;
  check(!Tab5Audio.execSetWifiEnabled(true), "station changed during direct sharing");
  Tab5FileShareNetworkTransition::stop(attempted); busy = false;
  check(WiFi.apStops == 1 && WiFi.getMode() == WIFI_OFF, "AP stop left driver active");
  check(Tab5Audio.execConnectWifi("test", "", false) && WiFi.getMode() == WIFI_STA, "station cannot reconnect after AP stop");
  reset(); WiFi.allowAp = false; attempted = false;
  check(Tab5FileShareNetworkTransition::start("Tab5-test", "test-password", attempted) != nullptr, "failed AP accepted");
  Tab5FileShareNetworkTransition::stop(attempted);
  check(WiFi.getMode() == WIFI_OFF && !tab5NetworkBusy() && !tab5NetworkEnabled(), "AP failure did not restore off state");
  reset(); attempted = false;
  check(!Tab5FileShareNetworkTransition::start("Tab5-test", "test-password", attempted), "AP start failed");
  WiFi.allowApStop = false;
  busy = true; ready = true; enabled = true; code = 123456;
  Context sharing;
  tab5FileShareStop();
  check(!enabled && busy, "public Stop released AP before cleanup");
  testDelayHook = recoverApDriver;
  finishSession(sharing, -1, attempted);
  testDelayHook = nullptr;
  check(!busy && WiFi.getMode() == WIFI_OFF && WiFi.apStops == 2 && code == 0 && !*lastError.load(),
        "real worker cleanup failed to retry, clear transient error and release ownership after OFF");
  check(Tab5Audio.execConnectWifi("test", "", false), "station cannot reconnect after failed AP shutdown recovers");
  reset(); attempted = false;
  check(!Tab5FileShareNetworkTransition::start("Tab5-test", "test-password", attempted), "AP start failed");
  busy = true; lastError = "Ouverture du partage impossible";
  finishSession(sharing, -1, attempted);
  check(strcmp(lastError.load(), "Ouverture du partage impossible") == 0 && !busy,
        "AP cleanup lost original server failure");
  puts("PASS: real connection/off/reconnect/scan/popup/AP flows, failure states, admission races, cancellation and timeout");
  return 0;
#endif
 } catch (const std::exception& error) {
  fprintf(stderr, "FAIL: %s\n", error.what()); return 1;
 }
}
