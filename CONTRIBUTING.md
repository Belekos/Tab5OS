# Contributing

Please discuss a substantial change before starting a broad refactor. Tab5OS prioritizes stability, predictable resource use and preservation of user files and settings.

For bug reports, include:

- Tab5OS version and Tab5 hardware revision, if known.
- Connected USB devices, SD format and the app involved.
- Minimal steps, expected behavior and observed behavior.
- Relevant serial diagnostics or a non-personal example file.

Never include passwords, Wi-Fi credentials, PINs, encryption keys or private documents.

For code changes, preserve the existing app and resource lifecycles, use the SDK/LVGL facilities where suitable, keep UI text translatable and build with the pinned toolchain. State what was actually tested and distinguish host tests from device tests. Do not claim that native ELF code is isolated from the firmware.

Read [NOTICE.md](NOTICE.md) before contributing or redistributing. The first-party license has not yet been selected, and the combined audio binary has a separate license compatibility question. No contributor license agreement is imposed by this document.

For build details, see [docs/BUILD.md](docs/BUILD.md). Existing engineering notes are retained in [firmware/CONTRIBUTING.md](firmware/CONTRIBUTING.md).
