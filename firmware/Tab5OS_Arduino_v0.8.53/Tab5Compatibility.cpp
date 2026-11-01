#include "Tab5Compatibility.h"

#include <M5Unified.h>
#include <esp_event.h>
#include <esp_chip_info.h>
#include <esp_hosted.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace Tab5Compatibility {
namespace {
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
State state({ESP_HOSTED_VERSION_MAJOR_1, ESP_HOSTED_VERSION_MINOR_1,
             ESP_HOSTED_VERSION_PATCH_1});
// Only beforeWifiStart(), on the single audio worker, accesses this handle.
esp_event_handler_instance_t eventHandler = nullptr;

void hostedEvent(void*, esp_event_base_t, int32_t id, void* data) {
  Event event;
  uint32_t resetReason = 0;
  switch (id) {
    case ESP_HOSTED_EVENT_CP_INIT:
      event = Event::CoprocessorInit;
      if (data) resetReason = static_cast<esp_hosted_event_init_t*>(data)->reason;
      break;
    case ESP_HOSTED_EVENT_TRANSPORT_UP: event = Event::Up; break;
    case ESP_HOSTED_EVENT_TRANSPORT_DOWN: event = Event::Down; break;
    case ESP_HOSTED_EVENT_TRANSPORT_FAILURE: event = Event::Failure; break;
    default: return;
  }
  portENTER_CRITICAL(&stateMux);
  state.event(event, resetReason);
  portEXIT_CRITICAL(&stateMux);
}
}  // namespace

void begin() {
  esp_chip_info_t chip = {};
  esp_chip_info(&chip);
  Display display = Display::Unknown;
  auto* touch = M5.Display.touch();
  if (M5.getBoard() == m5::board_t::board_M5Tab5 && touch && !touch->isSPI()) {
    const auto config = touch->config();
    uint8_t version = 0;
    bool obtained = false;
    if (config.i2c_addr == 0x55) {
      // The same read-only register used by M5GFX's Tab5 autodetection.
      // No broad I2C scan and no register writes or panel/touch reset.
      const uint8_t reg[] = {0, 0};
      obtained = lgfx::i2c::transactionWriteRead(config.i2c_port,
          config.i2c_addr, reg, sizeof(reg), &version, 1, 100000).has_value();
    }
    display = identifyDisplay(true, config.i2c_addr, obtained, version);
  }
  portENTER_CRITICAL(&stateMux);
  state.setDisplay(display);
#if defined(CONFIG_ESP_REV_MIN_FULL) && defined(CONFIG_ESP_REV_MAX_FULL)
  state.setChipRevision(chip.revision, CONFIG_ESP_REV_MIN_FULL, CONFIG_ESP_REV_MAX_FULL);
#endif
  portEXIT_CRITICAL(&stateMux);
}

void beforeWifiStart() {
  if (eventHandler) return;
  const auto result = esp_event_loop_create_default();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return;
  if (esp_event_handler_instance_register(ESP_HOSTED_EVENT, ESP_EVENT_ANY_ID,
      hostedEvent, nullptr, &eventHandler) != ESP_OK) {
    eventHandler = nullptr;
    return;
  }
  portENTER_CRITICAL(&stateMux);
  state.setEventMonitoring(true);
  portEXIT_CRITICAL(&stateMux);
}

Snapshot snapshot() {
  portENTER_CRITICAL(&stateMux);
  const auto result = state.snapshot();
  portEXIT_CRITICAL(&stateMux);
  return result;
}

const char* displayName(Display display) {
  switch (display) {
    case Display::Ili9881cGt911: return "ILI9881C / GT911";
    case Display::St7121: return "ST7121";
    case Display::St7123: return "ST7123";
    default: return "Non identifié";
  }
}

const char* firmwareMessage(Firmware firmware) {
  switch (firmware) {
    case Firmware::SameVersion: return "Versions ESP-Hosted concordantes";
    case Firmware::DifferentVersion: return "Versions ESP-Hosted différentes : compatibilité à vérifier";
    case Firmware::Unknown: return "Version du firmware C6 indisponible";
    default: return "Version du firmware C6 indisponible";
  }
}

const char* transportMessage(Transport transport) {
  switch (transport) {
    case Transport::Up: return "Transport C6 disponible";
    case Transport::Down: return "Transport C6 arrêté";
    case Transport::Failed: return "Échec du transport C6";
    default: return "Transport C6 non démarré";
  }
}
}  // namespace Tab5Compatibility
