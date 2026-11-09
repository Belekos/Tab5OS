# Applications and home screen

Tab5OS combines built-in tools with ELF and Python applications installed on microSD or USB. The home screen provides access to Explorer, the editor, Drawing, eBook, Media, Maps, Calculator, Settings, Terminal, Alarm, RTL-SDR and Meshtastic. The **ELF Apps** and **Python Apps** menus list applications found on the storage device; **F5** refreshes the catalog.

## ELF applications

Download and extract a complete SD package matching the firmware being installed. The [published SD download](../downloads/Tab5OS_0.8.53_SD.zip) is the earlier 0.8.53 build; the DOS candidate requires its updated firmware and complete package. On a new card, copy the **contents of the `sdcard` folder** to the microSD root. For an existing card, follow the [update procedure](../docs/INSTALL_EN.md#update-an-existing-microsd-card).

The repository's [sdcard/Applications](Tab5OS_Arduino_v0.8.53/sdcard/Applications/) contains 11 prebuilt ELF files. CI builds `DOS.app.elf` separately and includes it in the candidate's complete SD package, bringing that package to 12 applications. Copy the SD resources as described in the [installation guide](../docs/INSTALL_EN.md), then open **ELF Apps** or select a file in Explorer and choose **Open with**.

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
| DOS candidate | `DOS.app.elf` | No file for FreeDOS, or a compatible 16-bit `.COM` / `.EXE` program on SD; [guide](DOS.md) |
| Maze3D | `Maze3D.app.elf` | No file |
| File hash | `hash_file.app.elf` | File to read; non-cryptographic 32-bit FNV-1a result in the console |

Commercial games are not included. For a Mega Drive `.md` ROM, use **Open with → MegaDrive**, because this extension is also used for Markdown documents. Maze3D is self-contained: collect the three yellow keys and reach the green exit.

### Launching

The launcher lets you choose a file or folder and remembers a mode for each application: **Ask at launch**, **No file** or **Always this file**. An explicit launch through **Open with** uses the selected file. Pentest has a launch profile that requires no file.

| Storage | Path in the launcher | Path received by the application |
| --- | --- | --- |
| microSD | `sd:/Folder/file.ext` | `/Folder/file.ext` |
| USB | `usb:/Folder/file.ext` | `/USB/Folder/file.ext` |

Enter the path directly, even if it contains spaces. Recent items and favorites store application/file pairs in `/System/native-apps.json`.

### DOS programs

Install the DOS candidate's matching firmware and complete SD resources. Double-click a `.COM` or `.EXE` file in Explorer, or choose **Actions → Open with DOS**. The launcher selects `DOS.app.elf` and keeps the program's full path as one file argument, including spaces. Press **Run**. **Open with → DOS (16-bit)** is also available. Use the DOS **Keys** panel for touch input, or connect a USB keyboard; a physical keyboard is optional.

To open the FreeDOS prompt without selecting a program, open DOS from **ELF Apps** and run it without a file argument.

DOS programs must be on SD; copy them from USB storage first. Their canonical SD paths must be shorter than 256 UTF-8 bytes. This is a 16-bit DOS environment, not a Windows application launcher. Windows executables, 386 software and Sound Blaster/AdLib audio are unsupported. Batch files do not have an automatic Explorer association. The generic game controls below do not describe DOS keyboard input; use the [DOS guide](DOS.md) for its controls, included FreeDOS image and current limits.

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

Games using the shared input system also support touch controls and [USB gamepads](MANETTES_USB.md). Saves are stored under `/Saves`, in each application's folder. In Maze3D, **X** runs, **Backspace** displays the map and **Enter** creates a maze.

### Doom controls

At the title screen or during an automatic demo, press **Enter / START** or **Z/W / A** to open the menu. Press again to confirm the highlighted item. Use the arrows or D-pad to move through menus; **X / B** or **Backspace / SELECT** goes back. A held button performs one menu action: release it before confirming again.

| In-game action | Keyboard | Gamepad / touch control |
| --- | --- | --- |
| Move forward/backward, turn | Arrow keys | D-pad / arrows |
| Fire | Z or W | A |
| Run and strafe with the arrows | Hold X | Hold B |
| Use a door or switch | Enter | START |
| Change weapon | Backspace | SELECT |
| Map | A on QWERTY; Q on AZERTY | Y |
| Strafe left | Q on QWERTY; A on AZERTY | L |
| Strafe right | E | R |
| Open/close the Doom menu | Ctrl+R | Menu button on screen |
| Quick save | Ctrl+S | Save button on screen |
| Quit to Tab5OS | Escape | Quit button on screen |

The table uses Tab5OS gamepad assignments, which you can configure in **Settings > Interface > Gamepad**. Save before quitting; load a saved game from Doom's menu. **F1** opens the OS controls panel. Doom does not use Escape to open its menu because Tab5OS reserves it for quitting.

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
