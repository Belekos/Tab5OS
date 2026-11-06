# Tab5OS 0.8.53 libraries

The [build guide](../docs/BUILD.md) describes dependency installation and commands. External versions are pinned in [dependency-downloads.json](build/build_linux/dependency-downloads.json).

| Component | Version used |
| --- | --- |
| Arduino CLI | 1.5.1 |
| M5Stack core / ESP-IDF | 3.3.9 / 5.5.4 |
| M5Unified / M5GFX | 0.2.23 / 0.2.30 |
| LVGL | 9.6.0 |
| ArduinoJson | 7.4.3 |
| ESP8266Audio | Tag 2.4.2; upstream metadata 2.4.1 |
| Embedded MicroPython | 1.27.0, Tab5OS port |
| Git | libgit2 1.9.7, ESP32-P4 adapter |
| SSH / SFTP | libssh2 1.11.2_DEV, revision `18aebe08962c1878d84c373ad85caff25ec67771` |
| Zstandard / WebP | 1.5.7 / 1.6.0 |
| USB MSC storage | 1.1.4, P4 adaptations and SCSI synchronization |

## Build adaptations

The scripts in [build/build_linux](build/build_linux/) prepare isolated copies of dependencies. They verify the expected hashes before applying the LVGL, M5GFX, M5Unified, JPEG, FS, DSI and memory adaptations. Globally installed libraries and the original SDK are preserved.

LVGL uses its TLSF allocator with expandable PSRAM pools. M5Unified adaptations coordinate audio activation and the release of PCM buffers. The build guide describes the GCC and Clang paths and their validation limits.

## Included sources and resources

Embedded components and their adaptations are in [the sketch](Tab5OS_Arduino_v0.8.53/). Game and emulator cores are kept under [tools/native-apps](Tab5OS_Arduino_v0.8.53/tools/native-apps/), with provenance files and build recipes.

Fonts, CJK dictionaries and map data are SD resources separate from the firmware. Relevant generators are in [build/resources](build/resources/). The [complete SD package](../downloads/Tab5OS_0.8.53_SD.zip) supplies these resources, applications and the Factory reference. [ELF files](Tab5OS_Arduino_v0.8.53/sdcard/Applications/) and [Python scripts](Tab5OS_Arduino_v0.8.53/sdcard/MicroPython/) are also available directly.

The [third-party notices](LICENCES.md) and [license manifest](LICENCES-manifest.json) list the components and paths to their license texts. Each component's terms are preserved in its sources; also see [NOTICE.md](../NOTICE.md).
