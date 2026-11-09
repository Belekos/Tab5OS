# Tab5OS

**A portable workspace for M5Stack Tab5.**

[Installation](docs/INSTALL_EN.md) · [Applications](firmware/APPLICATIONS.md) · [Gallery](docs/screenshots/README.md)

![Tab5OS 0.8.53 — English interface](docs/screenshots/cover-en.png)

Tab5OS brings writing, drawing, reading, media, maps and development tools to the **M5Stack Tab5 / ESP32-P4**. Use its graphical interface with touch, a USB keyboard and a mouse. Documents and applications can live on microSD or USB storage.

## Features

| Activity | Tools |
| --- | --- |
| Write and read | Text/code editor, ebooks and offline Wikipedia with local archives |
| Draw | Brushes, layers, selections, shapes, text, transformations, undo/redo and T5I projects; BMP/JPEG export |
| Manage and share | SD/USB Explorer, “Open with” and local file sharing through a computer's browser |
| Listen and watch | Media player and web radio |
| Explore | Online/offline maps, place search and waypoints |
| Program | Terminal, Git, MicroPython, ELF applications, serial tools and the [16-bit DOS candidate](firmware/Tab5OS_Arduino_v0.8.53/DOS.md) |
| Connect | Remote SSH commands, SFTP, IRC and Meshtastic with a compatible external radio |
| Print and protect | IPP printing, supported USB printers, file encryption and a PIN-access vault |
| Play | Doom, Maze3D, Game Boy/Color, NES, Sega 8-bit, GBA, SNES and Mega Drive |
| Customize | Themes, interface languages, shortcut sidebar, calculator and alarm; RTL-SDR with a compatible USB receiver |

The **11 tracked ELF executables** are in [Applications](firmware/Tab5OS_Arduino_v0.8.53/sdcard/Applications/). The validation build compiles DOS separately as the twelfth application for the candidate's complete SD package. DOS requires matching firmware and SD resources; it is not included among those 11 prebuilt files. Supply commercial game ROMs and Wikipedia archives separately.

The DOS candidate opens compatible 16-bit `.COM` and `.EXE` programs from SD through **Explorer → Actions → Open with DOS**, then **Run**. Use its on-screen **Keys** panel, USB keyboard, USB mouse or touchpad controls. PC speaker, AdLib OPL2 and Sound Blaster 2.0 sound are supported, with volume and mute controls. Windows programs and 386/486 software remain unsupported in this 286 application; see the [DOS guide](firmware/Tab5OS_Arduino_v0.8.53/DOS.md) for controls, performance indicators and storage limits.

## Download and install 0.8.53

| File | Use |
| --- | --- |
| [OTA firmware](downloads/Tab5OS_0.8.53_OTA.bin?raw=1) | Update an existing Tab5OS installation |
| [First-install image](downloads/Tab5OS_0.8.53_initial_install_16MB.bin?raw=1) | Install from a computer; replaces internal Flash |
| [Complete microSD package](downloads/Tab5OS_0.8.53_SD.zip?raw=1) | Applications, resources and Factory recovery files |

The downloads above are the previously published 0.8.53 build. They do not contain the DOS candidate until an updated package is published. The current source also repairs language-pack validation and adds separate Explorer file/folder icon colors, alongside the Drawing relief and mouse palette fixes. [Build the current source](docs/BUILD.md), including its [DOS application and resources](firmware/COMPILATION.md#applications-and-resources), or use a complete package produced from the same validated candidate.

Follow the [installation guide](docs/INSTALL_EN.md) to choose the right image, prepare the card or update an existing installation. Use the supplied [SHA-256 checksums](downloads/SHA256SUMS.txt) to verify downloads.

The [gallery](docs/screenshots/README.md) shows the English interface rendered from the application source with LVGL on a computer.
