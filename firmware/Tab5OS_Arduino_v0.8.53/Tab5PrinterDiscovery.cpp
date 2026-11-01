#include "Tab5Task.h"
#include "Tab5PrinterDiscovery.h"
#include "Tab5UsbPrinter.h"
#include "Tab5PrintProtocol.h"
#include "Tab5NetworkGate.h"
#include <Preferences.h>
#include <WiFi.h>
#include <mdns.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>

namespace {
constexpr size_t MaxPrinters = 24, MaxName = 128, MaxUri = 255;
constexpr size_t MaxStored = 7 + MaxUri + MaxPrinters * (4 + MaxName + MaxUri);
std::atomic<bool> scanning{false}, ready{false};
std::atomic<const char*> message{"Prêt"};
// The UI owns saved/choice. The worker owns pending until it publishes ready
// and clears scanning; the UI consumes it before starting another worker.
std::vector<Tab5Printers::Printer> saved, pending;
std::vector<uint8_t> storedBytes;
String choice;
bool loaded = false, refreshSucceeded = false;

bool validUri(const String& value) {
  if(Tab5UsbPrinter::uri(value.c_str()))return true;
  tab5print::Endpoint endpoint;
  return value.length() <= MaxUri && tab5print::endpoint(value.c_str(), endpoint);
}
void append(std::vector<uint8_t>& data, const String& value) {
  const size_t n = value.length();
  data.push_back(n & 255); data.push_back(n >> 8);
  data.insert(data.end(), value.c_str(), value.c_str() + n);
}
bool readString(const std::vector<uint8_t>& data, size_t& at, size_t limit, String& value) {
  if (at + 2 > data.size()) return false;
  const size_t n = data[at] | (size_t(data[at + 1]) << 8); at += 2;
  if (n > limit || n > data.size() - at) return false;
  for (size_t i = 0; i < n; ++i) if (data[at + i] < 32 || data[at + i] == 127) return false;
  value = "";
  for (size_t i = 0; i < n; ++i) value += char(data[at++]);
  return true;
}
void load() {
  if (loaded) return;
  loaded = true;
  Preferences prefs;
  if (!prefs.begin("tab5printers", true)) return; // Absent on first use.
  const size_t n = prefs.getBytesLength("saved");
  std::vector<uint8_t> data;
  if (n >= 7 && n <= MaxStored) {
    data.resize(n);
    if (prefs.getBytes("saved", data.data(), n) != n) data.clear();
  }
  prefs.end();
  if (data.size() < 7 || memcmp(data.data(), "T5P1", 4) || data[4] > MaxPrinters) return;
  size_t at = 5;
  String last;
  if (!readString(data, at, MaxUri, last) || (last.length() && !validUri(last))) return;
  std::vector<Tab5Printers::Printer> entries;
  for (unsigned i = 0; i < data[4]; ++i) {
    String name, uri;
    if (!readString(data, at, MaxName, name) || !name.length() ||
        !readString(data, at, MaxUri, uri) || !validUri(uri)) return;
    for (const auto& previous : entries) if (previous.uri == uri) return;
    entries.push_back({name, uri, uri.startsWith("ipps://")});
  }
  if (at != data.size()) return;
  saved.swap(entries); choice = last; storedBytes.swap(data);
}
bool persist() {
  std::vector<uint8_t> data{'T','5','P','1',uint8_t(saved.size())};
  append(data, choice);
  for (const auto& printer : saved) { append(data, printer.name); append(data, printer.uri); }
  if (data == storedBytes) return true;
  Preferences prefs;
  if (!prefs.begin("tab5printers", false)) return false;
  const bool ok = prefs.putBytes("saved", data.data(), data.size()) == data.size();
  prefs.end();
  if (ok) storedBytes.swap(data);
  return ok;
}
String printerName(const char* value) {
  String name = value;
  for (size_t i = 0; i < name.length(); ++i)
    if (uint8_t(name[i]) < 32 || uint8_t(name[i]) == 127) name[i] = ' ';
  if (name.length() > MaxName) {
    size_t n = MaxName;
    while (n && (uint8_t(name[n]) & 0xc0) == 0x80) --n;
    name = name.substring(0, n);
  }
  return name;
}
void discover() {
  refreshSucceeded = false;
  if (!tab5NetworkEnabled()) { message = "Recherche réseau indisponible"; return; }
  if (WiFi.status() != WL_CONNECTED) { message = "Connecter le Tab5 au Wi-Fi"; return; }
  Tab5NetworkLease network("printer-discovery", 1000);
  if (!network) { message = "Recherche réseau indisponible"; return; }
  // mDNS owns its in-flight request. Do not tear down the SDK service to
  // cancel it: check the OS switch between queries (each bounded to 3 seconds).
  if (mdns_init() != ESP_OK) message = "Recherche réseau indisponible";
  else {
    bool queryFailed = false;
    for (bool tls : {false, true}) {
      if (!tab5NetworkEnabled()) { queryFailed = true; break; }
      mdns_result_t* entries = nullptr;
      if (mdns_query_ptr(tls ? "_ipps" : "_ipp", "_tcp", 3000, 12, &entries) != ESP_OK)
        queryFailed = true;
      for (auto* p = entries; p; p = p->next) {
        if (!p->hostname || !p->port) continue;
        String host = p->hostname;
        if (!host.endsWith(".local")) host += ".local";
        String resource;
        for (size_t i = 0; i < p->txt_count; ++i)
          if (p->txt[i].key && !strcmp(p->txt[i].key, "rp") && p->txt[i].value) resource = p->txt[i].value;
        if (!resource.length()) continue;
        if (!resource.startsWith("/")) resource = String("/") + resource;
        String uri = String(tls ? "ipps://" : "ipp://") + host + ":" + p->port + resource;
        if (!validUri(uri)) continue;
        bool duplicate = false;
        for (const auto& printer : pending) if (printer.uri == uri) duplicate = true;
        if (!duplicate && pending.size() < MaxPrinters) {
          String name = printerName(p->instance_name ? p->instance_name : p->hostname);
          if (name.length()) pending.push_back({name, uri, tls});
        }
      }
      mdns_query_results_free(entries);
    }
    if (!tab5NetworkEnabled()) queryFailed = true;
    refreshSucceeded = !queryFailed;
    message = queryFailed ? "Recherche réseau indisponible" : pending.empty() ?
      "Aucune imprimante annoncée sur ce réseau" : "Choisir une imprimante détectée";
  }
}
void task(void*) {
  // Return through discover() before self-deletion so the network lease is
  // released on its owning task and before publishing the result to the UI.
  discover();
  ready = true;
  scanning = false;
  tab5TaskCheckpoint("printers");vTaskDelete(nullptr);
}
}
namespace Tab5Printers {
void service() {
  load();
  if (scanning || !ready.exchange(false)) return;
  if (refreshSucceeded) {
    saved.swap(pending);
    if (!persist()) message = "Mémorisation des imprimantes impossible";
  }
  pending.clear();
}
bool search() {
  service();
  if (scanning.exchange(true)) return false;
  pending.clear(); message = "Recherche des imprimantes…";
  if (tab5CreateTask(task, "tab5_printers", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    scanning = false; message = "Mémoire insuffisante"; return false;
  }
  return true;
}
bool busy() { return scanning.load(); }
const char* status() { return message.load(); }
String selected() { service(); return choice; }
bool remember(const String& uri) {
  service();
  if (!validUri(uri)) return false;
  choice = uri;
  if (persist()) return true;
  message = "Mémorisation des imprimantes impossible";
  return false;
}
std::vector<Printer> results() {
 service();auto result=saved;
 if(Tab5UsbPrinter::ready())result.push_back({"Imprimante USB IPP","usb://printer",false});
 return result;
}
}
