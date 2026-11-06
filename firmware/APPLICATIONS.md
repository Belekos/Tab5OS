# Applications and home screen

Tab5OS combines built-in tools with ELF and Python applications installed on microSD or USB. The home screen provides access to Explorer, the editor, Drawing, eBook, Media, Maps, Calculator, Settings, Terminal, Alarm, RTL-SDR and Meshtastic. The **ELF Apps** and **Python Apps** menus list applications found on the storage device; **F5** refreshes the catalog.

## ELF applications

Download and extract the [complete SD package](../downloads/Tab5OS_0.8.53_SD.zip). On a new card, copy the **contents of the `sdcard` folder** to the microSD root. For an existing card, follow the [update procedure](../docs/INSTALL_EN.md#update-an-existing-microsd-card). Executables are also available in [sdcard/Applications](Tab5OS_Arduino_v0.8.53/sdcard/Applications/). Copy the SD resources as described in the [installation guide](../docs/INSTALL_EN.md), then open **ELF Apps** or select a file in Explorer and choose **Open with**.

| Application | Executable | File to open |
| --- | --- | --- |
| Pentest | `Pentest.app.elf` | No file; [guide](PENTEST.md) |
| Game Boy | `gameboy.app.elf` | `.gb` ROM |
| Game Boy Color | `gameboy_color.app.elf` | `.gbc` or `.gb` ROM |
| NES | `NES.app.elf` | `.nes` ROM |
| Sega 8-bit | `Sega8.app.elf` | Master System `.sms` or Game Gear `.gg` ROM |
| GBA | `GBA.app.elf` | `.gba` ROM |
| SNES | `SNES.app.elf` | `.sfc` or `.smc` ROM |
| Mega Drive | `MegaDrive.app.elf` | `.gen` or `.md` ROM |
| Doom | `Doom.app.elf` | PrBoom-compatible `.wad` IWAD |
| Maze3D | `Maze3D.app.elf` | No file |
| File hash | `hash_file.app.elf` | File to read; non-cryptographic 32-bit FNV-1a result in the console |

Commercial games are not included. For a Mega Drive `.md` ROM, use **Open with → MegaDrive**, because this extension is also used for Markdown documents. Maze3D is self-contained: collect the three yellow keys and reach the green exit.

### Launching

The launcher lets you choose a file or folder and remembers a mode for each application: **Ask at launch**, **No file** or **Always this file**. An explicit launch through **Open with** uses the selected file. Pentest has a launch profile that requires no file.

| Storage | Path in the launcher | Path received by the application |
| --- | --- | --- |
| microSD | `sd:/Dossier/fichier.ext` | `/Dossier/fichier.ext` |
| USB | `usb:/Dossier/fichier.ext` | `/USB/Dossier/fichier.ext` |

Enter the path directly, even if it contains spaces. Recent items and favorites store application/file pairs in `/System/native-apps.json`.

### Game controls

| Action | Keyboard |
| --- | --- |
| Directions | Arrow keys |
| A / B | Z or W / X |
| Select / Start | Backspace / Enter |
| Help and game-specific controls | F1 |
| Save state | Ctrl+S |
| Volume | Page Up / Page Down |
| Mute | M |
| Quit | Escape |

Games using the shared input system also support touch controls and [USB gamepads](MANETTES_USB.md). Saves are stored under `/Saves`, in each application's folder. In Doom, **Ctrl+R** opens the menu: save before quitting and load the game from that menu. In Maze3D, **X** runs, **Backspace** displays the map and **Enter** creates a maze.

Compatibility depends on the format, cartridge and emulator core. ELF programs must target the Tab5OS API: they share the OS memory. A PC program or a `.bin` firmware cannot be used as a Tab5OS ELF application.

## Python applications

The [MicroPython scripts](Tab5OS_Arduino_v0.8.53/sdcard/MicroPython/) can be launched from **Python Apps**, Explorer or the editor.

| Script | Purpose |
| --- | --- |
| `irc.py` | IRC chat; [guide](IRC.md) |
| `wikipedia.py` | Catalog and preparation for Wikipedia reading; [guide](WIKIPEDIA.md) |
| `libraries.py` | Modules and projects; [guide](MICROPYTHON_BIBLIOTHEQUES.md) |
| `import_maps.py` | Map imports; [guide](CARTES.md) |
| `import_radios.py` | Radio catalog imports |
| `create_theme.py` | Access to theme creation |
| `update_sd.py` | Resource repair and updates; [guide](REPARATION_SD.md) |
| `example.py`, `interface.py` | Script and interface examples |

Quitting a Python application returns to the previous screen. **Hide**, where available, keeps it running; **Ctrl+Alt+P** resumes the active application and **Ctrl+.** requests that it stop.

## Customize the home screen and sidebar

In **Settings → Interface → Display → Home and sidebar**, choose the visible applications, their order, symbols, and grid or list layout. **Save** keeps the home screen changes. **Alt+7** opens Settings even when its shortcut is hidden.

The sidebar can hold ten native, ELF or Python shortcuts. Removing a shortcut does not delete the program. A shortcut to a disconnected device remains saved and becomes usable again after reconnection.

## Add your own applications

| Location on SD or USB | Recognized content |
| --- | --- |
| `/MicroPython` | Registered applications and `.py` scripts directly in the folder |
| `/Applications` and `/Apps` | `.py`, `.elf`, `.app.elf` files directly in the folder |
| A subfolder of these locations | `main.py`, `main.app.elf` or `main.elf` |

The catalog does not run programs while scanning. It accepts at most 64 entries, including built-in tools; other files remain accessible through Explorer.

For a custom icon, place an ICO file beside the program, `icon.ico` in its folder, or an icon with the same name under `/Icons`. Use an embedded 24-bit or 32-bit BMP image, at most 64 × 64 pixels, in a file no larger than 32 KiB.

The [native sources and build recipes](Tab5OS_Arduino_v0.8.53/tools/native-apps/) accompany the applications. The [development guide](DEVELOPPEMENT.md) describes C/ELF and MicroPython projects. Drawing is built into the firmware; its [guide](DESSIN.md) describes the tools and image formats.
