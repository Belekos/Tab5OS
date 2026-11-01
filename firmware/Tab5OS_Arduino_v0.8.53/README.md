# Tab5OS 0.8.53 — Arduino sketch

See the parent folder's LISEZ_MOI.md, RAPPORT_FONCTIONNEL_0.8.53.md,
BIBLIOTHEQUES.md, BINAIRES.md and VERIFICATION.md for installation,
changes, dependencies and the scope of validation for this release.

| Component | Build version |
|---|---|
| M5Stack Arduino / ESP-IDF | 3.3.9 / 5.5.4 |
| M5Unified / M5GFX | 0.2.23 / 0.2.30 |
| LVGL | 9.6.0 |
| ArduinoJson | 7.4.3 |
| ESP8266Audio | 2.4.2 tag; upstream metadata still says 2.4.1 |
| MicroPython custom embedded port | 1.27.0 |

Follow COMPILATION.md in the parent folder to rebuild with the supplied scripts
and isolated library adaptations. The target is the pre-revision-3 ESP32-P4
Tab5, with PSRAM, DIO flash and the supplied partition table. The delivered BIN
is an OTA application image, not a merged image to flash at address zero.
Applications/gameboy.app.elf is built separately using the native SDK;
tools/native-apps contains its source and upstream licences.

Theme editing and file selection are native. Wikipedia HTML conversion is native; catalog controls remain in Python. The terminal runs an independent C++ command engine. Create .sh scripts in the text editor. See TERMINAL.md in the parent directory.

USB MSC 1.1.4 and gnuboy are vendored with their licences. Hardware ABI 3 is documented in tools/native-apps/HARDWARE.md. GBC and GBA are separate applications; see the parent BINAIRES.md for delivered engines and their limits.

Alarm clock and ZIM decoding are C++ services. See REVEIL.md and WIKIPEDIA.md in the parent folder. The RTC powers the Tab5 back on; alarm sound uses the common audio driver.
