# Build Tab5OS 0.8.53

The sketch is [firmware/Tab5OS_Arduino_v0.8.53](../firmware/Tab5OS_Arduino_v0.8.53/). Scripts are in [firmware/build](../firmware/build/). Generated resource headers needed to compile the firmware are included.

## Dependencies

| Component | Version / setting |
| --- | --- |
| Arduino CLI | 1.5.1 |
| M5Stack core / ESP-IDF | 3.3.9 / 5.5.4 |
| GCC RISC-V | 14.2.0, esp-rv32/2601 |
| M5Unified / M5GFX | 0.2.23 / 0.2.30 |
| LVGL / ArduinoJson | 9.6.0 / 7.4.3 |
| ESP8266Audio | tag 2.4.2, metadata 2.4.1 |
| Target | ESP32-P4 prev3, PSRAM enabled, 16 MiB Flash, DIO |

Use the supplied scripts. They prepare isolated LVGL, M5GFX, M5Unified, FS, DSI and DMA adaptations without changing globally installed libraries or flashing a device.

## Linux x86_64

Requires Python 3.10 or later, curl, tar, network access and about 10 GB free space. From the repository root:

```bash
cd firmware
TAB5_BUILD_DIR="$PWD/../.build-tab5os"
mkdir -p "$TAB5_BUILD_DIR"
curl -fL https://downloads.arduino.cc/arduino-cli/arduino-cli_1.5.1_Linux_64bit.tar.gz -o "$TAB5_BUILD_DIR/arduino-cli.tar.gz"
tar --no-same-owner -xzf "$TAB5_BUILD_DIR/arduino-cli.tar.gz" -C "$TAB5_BUILD_DIR" arduino-cli
python3 build/build_linux/prepare_linux.py --environment "$TAB5_BUILD_DIR"
python3 build/build_linux/verify_downloads.py --environment "$TAB5_BUILD_DIR"
python3 build/build_linux/compile_linux.py --release-root "$PWD" --environment "$TAB5_BUILD_DIR" --sketch-name Tab5OS_Arduino_v0.8.53 --jobs 4
python3 build/build_linux/verify_linux.py --environment "$TAB5_BUILD_DIR"
```

[dependency-downloads.json](../firmware/build/build_linux/dependency-downloads.json) pins download URLs and SHA-256 checksums. Outputs go to `.build-tab5os/build`; logs and verification results go to `.build-tab5os/qa`.

The verifier checks source stability, RISC-V ABI, image integrity, embedded ELF hash, partitions and linked adaptations. `--baseline` accepts an optional prior-build reference for memory comparisons.

## Windows

Install Python 3.10 or later and Arduino CLI, then run from the repository root:

```powershell
Set-Location firmware
.\Compiler_Tab5OS.ps1 -CheckOnly
.\Compiler_Tab5OS.ps1 -InstallDependencies
```

Subsequent builds use `.\Compiler_Tab5OS.ps1`. Use `-ArduinoCli` to select the CLI or `-SketchPath` to select a sketch. Results go to `firmware/build-environment`.

The [French guide](../firmware/COMPILATION.md) also describes the experimental Windows/Clang route.

## Partitions and applications

`PartitionScheme=default` selects the SDK's `tools/partitions/default_16MB.csv`, with two 6,553,600-byte application slots.

The [11 compiled ELF applications](../firmware/Tab5OS_Arduino_v0.8.53/sdcard/Applications/) are available beside their [sources and build recipes](../firmware/Tab5OS_Arduino_v0.8.53/tools/native-apps/). Rebuild them separately when changing application code.

The [complete SD package](../downloads/Tab5OS_0.8.53_SD.zip) includes ELF/Python applications, generated fonts and dictionaries, and the Factory repair reference. The source-tree [sdcard](../firmware/Tab5OS_Arduino_v0.8.53/sdcard/) exposes editable resources, Python scripts and ELF executables. [Resource tools](../firmware/build/resources/README.md) regenerate the corresponding firmware headers and SD data.

For installation using the supplied firmware images, follow the [installation guide](INSTALL_EN.md). Firmware build outputs and the SD package serve separate steps of installation.

See [NOTICE.md](../NOTICE.md) for component distribution information.
