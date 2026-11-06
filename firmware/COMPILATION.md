# Build Tab5OS 0.8.53

The sketch is in [Tab5OS_Arduino_v0.8.53](Tab5OS_Arduino_v0.8.53/), and the scripts are in [build](build/). These commands do not flash any device.

## Linux / GCC

The [complete procedure](../docs/BUILD.md#linux-x86_64) provides pinned dependencies, commands and verification steps. It requires Python 3.10 or later, Arduino CLI 1.5.1, M5Stack core 3.3.9 / ESP-IDF 5.5.4 and GCC RISC-V 14.2.0.

The libraries are M5Unified 0.2.23, M5GFX 0.2.30, LVGL 9.6.0, ArduinoJson 7.4.3 and ESP8266Audio tag 2.4.2 (metadata 2.4.1). [URLs and SHA-256 hashes](build/build_linux/dependency-downloads.json) are supplied.

`PartitionScheme=default` selects `tools/partitions/default_16MB.csv` from the M5Stack SDK, with two application slots of 6,553,600 bytes. The target uses `ChipVariant=prev3`, `PSRAM=enabled` and `FlashMode=dio`.

## Windows / GCC

Install Python and Arduino CLI, then open PowerShell in `firmware`:

```powershell
.\Compiler_Tab5OS.ps1 -CheckOnly
.\Compiler_Tab5OS.ps1 -InstallDependencies
```

The first command checks the sketch. The second downloads dependencies and builds it. Afterwards, use `.\Compiler_Tab5OS.ps1` without `-InstallDependencies`. The `-ArduinoCli` parameter accepts the CLI path; `-SketchPath` explicitly selects the sketch.

Outputs go into `build-environment`. The global Arduino installation remains intact.

## Adaptations and verification

The scripts prepare isolated copies of LVGL, M5GFX, M5Unified and FS, checking upstream hashes. They use the sketch's `lv_conf.h` and rebuild the native cache-safe DSI/DW-GDMA units. Keep the generated linker options and `--whole-archive` group: adding a definition in the IDE does not modify precompiled SDK archives.

The Linux verifier checks the current build files, image, partitions and linked adaptations. `--baseline` optionally compares memory sections with a reference file supplied separately.

## Windows / Clang

The [Clang scripts](build/build_clang/README.md) provide an experimental path. This path is not validated for 0.8.53 and does not yet reproduce all the GCC settings. Use Linux/GCC for the reference rebuild.

## Applications and resources

The [11 ELF executables](Tab5OS_Arduino_v0.8.53/sdcard/Applications/) are supplied with their [sources and recipes](Tab5OS_Arduino_v0.8.53/tools/native-apps/). They are rebuilt separately according to their ABI and dependencies. The [Python scripts](Tab5OS_Arduino_v0.8.53/sdcard/MicroPython/) can be edited directly.

Generated headers required by the firmware are included. The [resource utilities](build/resources/README.md) regenerate them or prepare external data. The [complete SD package](../downloads/Tab5OS_0.8.53_SD.zip) supplies applications, generated resources and the Factory reference. The [installation guide](../docs/INSTALL_EN.md) explains how to use the supplied BIN files and prepare the card.
