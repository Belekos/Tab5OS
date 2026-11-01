# Build Tab5OS 0.8.53

The original source layout is preserved below `firmware/`. Start from that directory. The included generated resource headers are sufficient for the firmware build; the source repository intentionally omits precompiled ELF applications and large generated SD resources.

## Verified toolchain

| Component | Version / setting |
| --- | --- |
| Arduino CLI | 1.5.1 |
| M5Stack Arduino core / ESP-IDF | 3.3.9 / 5.5.4 |
| GCC RISC-V | 14.2.0, esp-rv32/2601 |
| M5Unified / M5GFX | 0.2.23 / 0.2.30 |
| LVGL / ArduinoJson | 9.6.0 / 7.4.3 |
| ESP8266Audio | tag 2.4.2; upstream metadata says 2.4.1 |
| Target | ESP32-P4 `prev3`, PSRAM enabled, 16 MiB Flash, DIO |

Use the supplied build scripts rather than an unadapted Arduino IDE build. They prepare isolated LVGL, M5GFX, M5Unified, FS, DSI and DMA adaptations without changing the globally installed SDK.

## Linux x86_64

Prerequisites: Python 3, curl, tar, network access for official pinned dependencies and about 10 GB of free build space.

From the repository root:

```bash
cd firmware
TAB5_BUILD_DIR="$PWD/../.build-tab5os"
mkdir -p "$TAB5_BUILD_DIR"
curl -fL https://downloads.arduino.cc/arduino-cli/arduino-cli_1.5.1_Linux_64bit.tar.gz -o "$TAB5_BUILD_DIR/arduino-cli.tar.gz"
tar --no-same-owner -xzf "$TAB5_BUILD_DIR/arduino-cli.tar.gz" -C "$TAB5_BUILD_DIR" arduino-cli
python3 tools/build_linux/prepare_linux.py --environment "$TAB5_BUILD_DIR"
python3 tools/build_linux/verify_downloads.py --environment "$TAB5_BUILD_DIR"
python3 tools/build_linux/compile_linux.py --release-root "$PWD" --environment "$TAB5_BUILD_DIR" --sketch-name Tab5OS_Arduino_v0.8.53_Ecriture --jobs 4
python3 tools/build_linux/verify_linux.py --environment "$TAB5_BUILD_DIR"
```

These commands compile and inspect the result; they do not flash a device. Outputs are in `.build-tab5os/build/` and `.build-tab5os/qa/`. Dependency URLs and expected SHA-256 hashes are in [dependency-downloads.json](../firmware/tools/build_linux/dependency-downloads.json).

The original 0.8.53 delivery passed this Linux/GCC build. The repository packaging has been checked against the original source bytes; no additional hardware or clean-room rebuild is claimed solely from repackaging it.

## Partition table correction

There is **no `partitions.csv` in this sketch**. The effective table is the M5Stack SDK's `tools/partitions/default_16MB.csv`, selected by the Tab5 board's `PartitionScheme=default` build setting. It provides two 6,553,600-byte application slots. The old delivery guide's phrase “table supplied in the sketch” was inaccurate; this publication guide corrects that description without changing the table or firmware.

## Windows and native applications

The original [COMPILATION.md](../firmware/COMPILATION.md) documents the retained PowerShell/GCC and Clang routes. Those Windows routes were not rerun for 0.8.53. Read the partition correction above when consulting that historical guide.

External native applications are built separately from [tools/native-apps](../firmware/Tab5OS_Arduino_v0.8.53_Ecriture/tools/native-apps/). They must use the Tab5OS ABI and their own applicable licenses. This repository includes their source and recipes, not their precompiled `.elf` files.

## Tests and resources

Host test sources and fixtures are retained under `firmware/tools/`; each suite has its own dependencies and runner. Comparative historical suites may require the authenticated baseline archive and complete historical evidence that are not included here. Do not interpret absent archived logs as newly successful tests.

Large SD resources, Factory copies and precompiled applications are omitted according to the [manifest](publication/source-manifest.json). A complete SD installation must be prepared from the matching resource recipes and legitimately distributable application builds; the partial source-tree `sdcard` directory must not be presented as the complete user installation package.

Review [NOTICE.md](../NOTICE.md) before distributing a locally built firmware. The audio license compatibility question is independent of whether a compilation succeeds.
