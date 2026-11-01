# Install Tab5OS 0.8.53 on M5Stack Tab5

**Availability:** this repository currently provides source code. Precompiled firmware, a complete SD installation package and a Tab5OS M5Burner listing are not yet published. The image names below describe the prepared release formats; they are not download links.

This firmware targets **M5Stack Tab5 / ESP32-P4 with 32 MiB PSRAM and 16 MiB Flash**. It does not install or replace the ESP32-C6 companion firmware.

## Choose the installation method

| Situation | Image | Method |
| --- | --- | --- |
| Tab5OS already installed | `Tab5OS_0.8.53_OTA.bin` | Built-in firmware updater |
| First installation | `Tab5OS_0.8.53_initial_install_16MB.bin` | Merged image written at `0x0` |

**The first-install image replaces the entire P4 Flash, including internal settings and data. Use the OTA image to update an existing installation.** Neither image contains the microSD resources.

Before installation, back up personal documents, settings and encryption keys as applicable. Keep power connected during firmware writing. When a binary package is available, verify its files against its supplied SHA-256 checksums.

## Update an existing installation

1. Copy the matching OTA image to the microSD card.
2. Select it in Explorer, then choose **Actions → Install firmware**.
3. Choose **Verify** and compare the displayed SHA-256 with the package checksum.
4. Choose **Install**, wait for success, then **Restart**.
5. Update the SD resources using the procedure below.

This writes the application into the other OTA slot and retains the partition table and NVS. Never write the OTA image at address `0x0`.

## First installation with M5Burner

These steps apply once the Tab5OS listing and complete SD package are published.

1. Prepare a FAT32 microSD card. Copy the **contents** of the complete package's `sdcard` folder to the card root.
2. Connect the Tab5 USB-C port with a data cable. Hold Reset for about two seconds, until the green LED flashes rapidly, then release it to enter download mode.
3. In M5Burner, select **Tab5OS 0.8.53**, the Tab5 device and its serial port.
4. Write the merged first-install image at `0x0`. Its P4 bootloader is already included at `0x2000`; no separate full-chip erase is required.
5. Restart with the prepared card inserted, then configure language and Wi-Fi in Tab5OS.

Use a M5Burner version supporting Tab5 / ESP32-P4. M5Burner import and physical boot of the prepared package have not been tested.

## Prepare or update the microSD card

Use a **complete resource package matching version 0.8.53**. The source tree's `firmware/Tab5OS_Arduino_v0.8.53/sdcard` is only a partial set: it omits compiled applications and some generated resources. Copying that folder alone does not provide a complete installation.

The full package supplies external ELF/Python applications, languages, fonts, keyboards and offline resources. Wikipedia archives, map tiles and game data may require separate preparation. Essential built-in tools remain accessible without a card.

For a new card, copy the contents of the complete package's `sdcard` folder to its root. For an existing card:

1. Back up personal files, custom modules, settings, keys, Wikipedia archives, game data and saves.
2. Copy the package's `sdcard/Factory/Tab5OS-0.8.53` folder to `/Factory/Tab5OS-0.8.53` on the card.
3. Update `MicroPython/lib/tab5factory.py`, `tab5workshop.py`, `tab5fs.py`, `tab5gui.py`, `tab5ui.py`, `tab5apps.py` and `MicroPython/update_sd.py` from the package, preserving any customized copies first.
4. Restart, open `/MicroPython/update_sd.py` in Explorer and select the normal **repair and update** action.
5. Restart again to load updated languages, fonts and keyboard layouts.

**Do not replace the whole `/System` directory.** The separate **restore modified files** action deliberately replaces customized resources. The setting for restoring missing files only recreates small absent resources; it does not update existing files or supply all external data.

## Install from source

The [build guide](BUILD.md) describes the required dependencies, compilation scripts and outputs. Building the firmware does not automatically create a complete SD package or an initial-install image.

Official references: [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro) · [Tab5 download mode](https://docs.m5stack.com/en/uiflow2/Tab5/program).
