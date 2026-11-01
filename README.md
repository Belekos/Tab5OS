# Tab5OS

**Writing, drawing and development on M5Stack Tab5.**

[Français](README.fr.md) · [Installation](docs/INSTALL_EN.md) · [Build from source](docs/BUILD.md) · [Gallery](docs/screenshots/README.md) · [0.8.53 changes](RELEASE_NOTES.md)

![Tab5OS interface previews](docs/screenshots/cover-en.png)

Tab5OS is a graphical firmware for the **M5Stack Tab5 / ESP32-P4**. It brings documents, creative tools, files, media and programmable applications together in an interface usable with touch, a USB keyboard and a mouse.

This repository contains the **0.8.53 source snapshot** and its publication documentation. The previews above are **LVGL renders on a computer**, with demonstration data; they are not photographs or captures from a physical tablet. [Capture provenance](docs/screenshots/manifest.json) is provided.

## What it includes

| Workspace | Tools |
| --- | --- |
| Write and read | Text/code editor, ebooks and local Wikipedia resources |
| Draw | Layers, selections, brushes, text, fills, history and T5I projects; BMP/JPEG export |
| Manage files | SD/USB Explorer, contextual actions and application selection |
| Listen and explore | Media playback, web radio, online/offline maps and waypoints |
| Program | Terminal, Git tools, MicroPython and native ELF applications |
| Connect and customize | Meshtastic, USB tools, RTL-SDR, alarm clock, themes and 14 interface languages |

The native home includes Explorer, Editor, eBook, Media, Maps, Calculator, Settings, Terminal, Alarm, RTL-SDR, Drawing and Meshtastic. The sidebar can hold up to **ten native, ELF or Python shortcuts without scrolling**. External applications are discovered on supported SD/USB locations.

Meshtastic requires a compatible external radio; RTL-SDR requires a compatible USB receiver. Native ELF applications must target the Tab5OS ABI and share the firmware's address space: there is no process isolation.

## Install or update

Start with the [installation guide](docs/INSTALL_EN.md). It distinguishes two different paths:

- **Existing Tab5OS installation:** use the application OTA image with the built-in updater to retain NVS and the partition table.
- **First installation:** use the merged P4 image only when replacing the device's internal Flash is intended.

The matching microSD resources are required for external applications, fonts, language resources and offline data. Do not replace the whole `/System` directory on a card containing personal settings.

**Publication status:** this is a source-only repository. It does not provide precompiled firmware or ELF downloads, and it does not claim that a Tab5OS M5Burner listing is already available. The first-party license decision and the linked audio components' license compatibility are still being resolved. See [NOTICE.md](NOTICE.md) and the [publication audit](docs/publication/AUDIT_PUBLICATION_0.8.53.md).

## Build and project layout

The original project is kept under [`firmware/`](firmware/) so its sketch, scripts and relative paths remain together. The current sketch is [`firmware/Tab5OS_Arduino_v0.8.53_Ecriture`](firmware/Tab5OS_Arduino_v0.8.53_Ecriture/).

The verified build uses M5Stack Arduino **3.3.9 / ESP-IDF 5.5.4**, GCC RISC-V **14.2.0**, LVGL **9.6.0**, M5Unified **0.2.23** and M5GFX **0.2.30**. Use the supplied scripts: they prepare isolated dependency adaptations needed by this firmware. [Build instructions](docs/BUILD.md) include the exact command sequence.

See [repository layout](docs/REPOSITORY_LAYOUT.md) for included and omitted files. Compiled images, debug ELF/map files, duplicate Factory payloads and bulky historical logs are intentionally outside this source repository. Source files, third-party notices and build recipes retain their original bytes; the [source manifest](docs/publication/source-manifest.json) records their origin.

## Validation

The original 0.8.53 release passed a complete ESP32-P4 build and targeted host tests. Its linked application image was 6,113,184 bytes, with 440,416 bytes remaining in its OTA slot. Static internal DATA+BSS measured 91,508 bytes, 2,620 bytes below 0.8.52. These are linked-section measurements, not live free-heap measurements.

[Verification details](firmware/VERIFICATION.md) distinguish executed checks, a pre-existing obsolete pagination-test expectation and tests still requiring hardware. No physical Tab5 test or M5Burner import is claimed for this publication preparation. The [device test procedure](firmware/TESTS_TAB5_0.8.53.md) covers input, shortcuts, memory and peripherals.

## Contribute

Read [CONTRIBUTING.md](CONTRIBUTING.md). Reports should include the version, device revision, connected peripherals, reproducible steps and relevant diagnostics without passwords, PINs or keys. Stability and preservation of user data take priority over new features.

Third-party components retain their own licenses. Do not apply one license label to the entire tree; see [NOTICE.md](NOTICE.md).
