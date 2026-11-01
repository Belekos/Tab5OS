# Contributing

Tab5OS prioritizes stability, predictable resource use and preservation of user files and settings. Discuss substantial changes before starting a broad refactor.

For bug reports, include:

- Tab5OS version and Tab5 hardware revision, if known.
- Connected peripherals, storage format and affected application.
- Reproduction steps, expected behavior and observed behavior.
- Relevant diagnostics or a sample file without personal information.

Do not include passwords, Wi-Fi credentials, PINs, encryption keys or private documents.

For code changes, preserve existing application lifecycles, make UI text translatable and follow the [build guide](docs/BUILD.md). State which checks were performed and distinguish computer tests from physical-device tests. External native applications share memory with the firmware.

Read [NOTICE.md](NOTICE.md) for licensing information. Application APIs and examples are described in [firmware/DEVELOPPEMENT.md](firmware/DEVELOPPEMENT.md).
