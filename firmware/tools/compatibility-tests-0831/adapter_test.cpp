#include "Tab5Compatibility.h"
#include "M5Unified.h"
#include "esp_hosted.h"
#include "freertos/portmacro.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <thread>

using namespace Tab5Compatibility;
static std::atomic<unsigned> checks{0};
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); abort(); } } while(0)
FakeM5 M5;
static unsigned reads = 0, registrations = 0, loopCalls = 0;
static uint8_t touchVersion = 3;
static bool readOk = true;
static int loopResult = ESP_OK, registerResult = ESP_OK;
static esp_event_handler_t handler = nullptr;
static void emit(int id, uint32_t reason = 0) {
  CHECK(handler != nullptr);
  esp_hosted_event_init_t event{reason};
  handler(nullptr, ESP_HOSTED_EVENT, id, &event);
}
lgfx::i2c::Result lgfx::i2c::transactionWriteRead(int port,int addr,const uint8_t* tx,
    size_t txlen,uint8_t* rx,size_t rxlen,int speed) {
  ++reads;
  CHECK(compatibilityCriticalDepth == 0);
  CHECK(port == 1 && addr == 0x55 && speed == 100000);
  CHECK(txlen == 2 && tx[0] == 0 && tx[1] == 0 && rxlen == 1);
  *rx = touchVersion;
  return {readOk};
}
int esp_event_loop_create_default() { ++loopCalls; return loopResult; }
int esp_event_handler_instance_register(esp_event_base_t base,int32_t id,
    esp_event_handler_t callback,void* arg,esp_event_handler_instance_t* result) {
  ++registrations;
  CHECK(base == ESP_HOSTED_EVENT && id == ESP_EVENT_ANY_ID && arg == nullptr);
  if (registerResult) return registerResult;
  handler = callback;
  *result = reinterpret_cast<void*>(1);
  return ESP_OK;
}
// Deliberately no esp_hosted_get_coprocessor_fwversion definition. Linking the
// actual adapter must succeed without that RPC; its declaration remains visible.

int main() {
  begin(); CHECK(reads == 0 && snapshot().display == Display::Unknown);
  FakeTouch touch; M5.Display.active = &touch;
  M5.board = m5::board_t::Other;
  begin(); CHECK(reads == 0 && snapshot().display == Display::Unknown);
  M5.board = m5::board_t::board_M5Tab5;
  touch.spi = true;
  begin(); CHECK(reads == 0 && snapshot().display == Display::Unknown);
  touch.spi = false;
  touch.settings.i2c_addr = 0x14;
  begin(); CHECK(reads == 0 && snapshot().display == Display::Ili9881cGt911);
  touch.settings.i2c_addr = 0x55;
  for (unsigned version = 0; version < 256; ++version) {
    touchVersion = static_cast<uint8_t>(version); readOk = true;
    begin();
    CHECK(snapshot().display == (version == 1 ? Display::St7121 :
          version == 3 ? Display::St7123 : Display::Unknown));
  }
  touchVersion = 1; readOk = false; begin();
  CHECK(snapshot().display == Display::Unknown);
  const auto readCount = reads;
  touch.settings.i2c_addr = 0x5d; begin();
  CHECK(reads == readCount && snapshot().display == Display::Unknown);
  const auto chip = snapshot();
  CHECK(chip.chipRevisionRangeKnown && chip.chipRevision == 103 &&
        chip.minimumChipRevision == 1 && chip.maximumChipRevision == 199);

  loopResult = 257; beforeWifiStart();
  CHECK(registrations == 0 && !snapshot().eventMonitoring);
  loopResult = ESP_ERR_INVALID_STATE; registerResult = 257; beforeWifiStart();
  CHECK(registrations == 1 && !snapshot().eventMonitoring);
  registerResult = ESP_OK; beforeWifiStart();
  CHECK(registrations == 2 && snapshot().eventMonitoring);
  for (int n = 0; n < 1000; ++n) beforeWifiStart();
  CHECK(registrations == 2 && loopCalls == 3);
  emit(ESP_HOSTED_EVENT_CP_INIT,1);
  emit(ESP_HOSTED_EVENT_TRANSPORT_UP);
  CHECK(snapshot().firmware == Firmware::NotChecked);
  CHECK(snapshot().transport == Transport::Up && snapshot().coprocessorResetReason == 1);
  emit(ESP_HOSTED_EVENT_TRANSPORT_FAILURE);
  CHECK(snapshot().transport == Transport::Failed && snapshot().transportFailures == 1);
  CHECK(snapshot().coprocessor.major == 0);
  emit(ESP_HOSTED_EVENT_TRANSPORT_DOWN);
  CHECK(snapshot().transport == Transport::Down && snapshot().transportFailures == 1);
  emit(ESP_HOSTED_EVENT_CP_HEARTBEAT);
  CHECK(snapshot().transport == Transport::Down);

  // Different major versions are diagnostic only, just like minor/patch changes.
  for (uint32_t major = 1; major <= 4; ++major)
    for (uint32_t minor = 0; minor <= 16; ++minor)
      for (uint32_t patch = 0; patch <= 32; ++patch) {
        const auto actual = compareVersions({2,12,8},{major,minor,patch},true);
        CHECK(actual == (major == 2 && minor == 12 && patch == 8 ?
                         Firmware::SameVersion : Firmware::DifferentVersion));
        CHECK(compareVersions({2,12,8},{major,minor,patch},false) == Firmware::Unknown);
      }
  for (const Version invalid : {Version{0,0,0}, {256,1,1}, {2,256,1}, {2,1,256}})
    CHECK(compareVersions({2,12,8},invalid,true) == Firmware::Unknown);
  CHECK(compareVersions({0,0,0},{2,12,8},true) == Firmware::Unknown);
  CHECK(std::strstr(firmwareMessage(Firmware::DifferentVersion), "bloqu") == nullptr);

  // No device access from cached reads, including concurrent transport events.
  std::thread events([] {
    for (unsigned n = 0; n < 10000; ++n) {
      emit(ESP_HOSTED_EVENT_TRANSPORT_FAILURE);
      emit(ESP_HOSTED_EVENT_TRANSPORT_UP);
    }
  });
  std::thread ui([] {
    for (unsigned n = 0; n < 10000; ++n) {
      const auto value = snapshot();
      CHECK(value.host.major == 2 && value.host.minor == 12 && value.host.patch == 8);
      CHECK(value.coprocessor.major == 0 && value.firmware == Firmware::NotChecked);
    }
  });
  events.join(); ui.join();
  CHECK(snapshot().transportFailures == 10001);
  CHECK(reads == readCount);
  printf("PASS %u adapter checks; display_variants=3; concurrent_events=20000; cached_reads=10000; version_RPC=absent_at_link\n", checks.load());
}
