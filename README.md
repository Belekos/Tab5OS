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
| Program | Terminal, Git, MicroPython, ELF applications and serial tools |
| Connect | Remote SSH commands, SFTP, IRC and Meshtastic with a compatible external radio |
| Print and protect | IPP printing, supported USB printers, file encryption and a PIN-access vault |
| Play | Doom, Maze3D, Game Boy/Color, NES, Sega 8-bit, GBA, SNES and Mega Drive |
| Customize | Themes, interface languages, shortcut sidebar, calculator and alarm; RTL-SDR with a compatible USB receiver |

The **11 ready-to-run ELF applications** are in [Applications](firmware/Tab5OS_Arduino_v0.8.53/sdcard/Applications/). They are also included in the SD package alongside Python applications, fonts, languages, keyboards and resources. Supply commercial game ROMs and Wikipedia archives separately.

## Download and install 0.8.53

| File | Use |
| --- | --- |
| [OTA firmware](downloads/Tab5OS_0.8.53_OTA.bin?raw=1) | Update an existing Tab5OS installation |
| [First-install image](downloads/Tab5OS_0.8.53_initial_install_16MB.bin?raw=1) | Install from a computer; replaces internal Flash |
| [Complete microSD package](downloads/Tab5OS_0.8.53_SD.zip?raw=1) | Applications, resources and Factory recovery files |

The downloads above are the published 0.8.53 build. The current source also fixes Drawing relief colors and mouse palette selection (left click: color 1; right click: color 2). [Build the current source](docs/BUILD.md) to use these fixes.

Follow the [installation guide](docs/INSTALL_EN.md) to choose the right image, prepare the card or update an existing installation. Use the supplied [SHA-256 checksums](downloads/SHA256SUMS.txt) to verify downloads.

The [gallery](docs/screenshots/README.md) shows the English interface rendered from the application source with LVGL on a computer.
