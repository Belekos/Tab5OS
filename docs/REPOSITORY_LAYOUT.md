# Repository layout

| Path | Contents |
| --- | --- |
| `README.md`, `README.fr.md` | Presentation, features, previews and installation entry points |
| `docs/INSTALL_EN.md`, `docs/INSTALL_FR.md` | First installation, OTA updates and SD preparation |
| `docs/BUILD.md` | Firmware compilation instructions |
| `docs/screenshots/` | Computer-rendered LVGL previews and their provenance |
| `firmware/Tab5OS_Arduino_v0.8.53/` | Arduino sketch, native services and embedded libraries |
| `firmware/Tab5OS_Arduino_v0.8.53/tools/native-apps/` | External application sources and build recipes |
| `firmware/Tab5OS_Arduino_v0.8.53/sdcard/` | Python applications and partial SD resources |
| `firmware/examples/` | Application examples |
| `firmware/*.md` | Application and development documentation |
| `NOTICE.md` | Licensing information and third-party notices |

The Arduino folder and its main `.ino` file share the name `Tab5OS_Arduino_v0.8.53`.

Precompiled firmware, native application binaries, complete generated SD resources and duplicate Factory payloads are distributed separately from the source tree. The included `sdcard` directory is not a complete installation package.

Historical reports, test output and the former `firmware/tools` directory are removed from the published layout. The required compilation helpers are documented in [BUILD.md](BUILD.md). External ELF/Python application sources remain part of the project.
