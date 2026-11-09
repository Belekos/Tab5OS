# Install Tab5OS 0.8.53

Tab5OS runs on M5Stack Tab5 / ESP32-P4 with 32 MiB PSRAM and 16 MiB Flash.

## Downloads

| File | Use |
| --- | --- |
| [OTA firmware](../downloads/Tab5OS_0.8.53_OTA.bin) | Update Tab5OS through its Explorer |
| [16 MiB initial installation](../downloads/Tab5OS_0.8.53_initial_install_16MB.bin) | Write the merged image at address `0x0` |
| [Complete SD package](../downloads/Tab5OS_0.8.53_SD.zip) | ELF/Python applications, fonts, languages, keyboards and Factory reference |
| [SHA256SUMS.txt](../downloads/SHA256SUMS.txt) | Verify all three downloads |

These links refer to the previously published 0.8.53 build. The DOS candidate requires the matching newly built firmware and complete SD package; the older downloads do not gain DOS support merely by copying an application. Its complete package includes 12 ELF applications, including the CI-built `DOS.app.elf`, and the DOS resources described in the [DOS guide](../firmware/Tab5OS_Arduino_v0.8.53/DOS.md).

**Initial installation replaces the entire P4 Flash, including internal settings and keys. Use the OTA firmware to update an existing installation.** Back up documents, settings and portable vault exports before installation. Keep power connected while writing. The ESP32-C6 companion retains its firmware.

In PowerShell, calculate a downloaded file’s checksum with `Get-FileHash -Algorithm SHA256 .\Tab5OS_0.8.53_SD.zip` and compare it with `SHA256SUMS.txt`. On Linux, place all three files and the checksum file in one directory, then run `sha256sum -c SHA256SUMS.txt`.

## First installation

1. Extract the SD package. Copy the **contents** of its `sdcard` folder to the root of a FAT32 microSD card: `Applications`, `MicroPython`, `System`, `Factory`, etc.
2. Install Python 3.10 or later and esptool 5:

   ```powershell
   python -m pip install "esptool>=5,<6"
   ```

3. Connect the Tab5 USB-C port using a data cable. Hold Reset for about two seconds until the green LED flashes rapidly, then release it.
4. Open a terminal in the directory containing the initial-install BIN. Replace `COM5` with the Tab5 serial port, then run:

   ```powershell
   python -m esptool --chip esp32p4 --port COM5 write-flash 0x0 Tab5OS_0.8.53_initial_install_16MB.bin
   ```

5. Wait for writing and verification to finish, then restart with the prepared microSD card inserted. Choose your language and configure Wi-Fi in **Settings**.

On Linux or macOS, use `python3` if needed and the corresponding port, such as `/dev/ttyACM0`. The merged image includes the bootloader, partition table and application; write it in one operation at `0x0`.

## Update Tab5OS

1. Copy [Tab5OS_0.8.53_OTA.bin](../downloads/Tab5OS_0.8.53_OTA.bin) to the microSD card.
2. Select this file in Explorer, then choose **Actions → Install firmware**.
3. Choose **Verify** and compare the displayed SHA-256 with [SHA256SUMS.txt](../downloads/SHA256SUMS.txt).
4. Choose **Install**, wait for success, then **Restart**.
5. Update the SD resources as described below.

The updater retains the partition table and internal NVS settings. The OTA BIN is intended for this procedure; do not write it at `0x0`.

## Update an existing microSD card

The [complete SD package](../downloads/Tab5OS_0.8.53_SD.zip) includes `Factory/Tab5OS-0.8.53`, the reference used by the repair tool.

1. Back up personal files, customized scripts, settings, keys, archives, games and saves.
2. Copy the package’s `sdcard/Factory/Tab5OS-0.8.53` folder to `/Factory/Tab5OS-0.8.53` on the card.
3. Update `MicroPython/lib/tab5factory.py`, `tab5workshop.py`, `tab5fs.py`, `tab5gui.py`, `tab5ui.py`, `tab5apps.py` and `MicroPython/update_sd.py` from the package, preserving any customized copies first.
4. Restart, open `/MicroPython/update_sd.py` and choose **Repair and update**.
5. Restart to load the updated resources.

**Keep your personal `/System` directory.** Normal repair preserves customized resources and valid settings; the separate **Restore modified files** action replaces them after confirmation. The [SD repair guide](../firmware/REPARATION_SD.md) describes these rules.

The [11 tracked ELF applications](../firmware/Tab5OS_Arduino_v0.8.53/sdcard/Applications/) and [Python scripts](../firmware/Tab5OS_Arduino_v0.8.53/sdcard/MicroPython/) are also directly available. DOS is built separately by CI and supplied in the candidate's complete package; it is not a twelfth prebuilt download in that source directory. Use the matching ZIP to prepare a card with all resources. The [Wikipedia](../firmware/WIKIPEDIA.md), [Maps](../firmware/CARTES.md) and [Applications](../firmware/APPLICATIONS.md) guides describe how to prepare archives, tiles and game data.

## Use the DOS candidate

Install the candidate firmware and SD package together, including `Applications/DOS.app.elf`, the `DOS` folder and `System/DOS/DRIVERS.IMG` from the same package. Put a compatible 16-bit DOS `.COM` or `.EXE` program on SD, select it in Explorer and choose **Actions → Open with DOS**, then **Run**. Full paths may contain spaces. Programs on USB storage must first be copied to SD. Use the DOS **Keys** panel, a USB keyboard, a USB mouse or the touchscreen touchpad controls.

Tap **Mouse** (**Souris** in French) in the OS toolbar to capture the USB mouse; tap it on the touchscreen again to release it. **Vol-** and **Vol+** adjust sound, and tapping the percentage mutes or restores it. PC speaker, AdLib OPL2 and Sound Blaster 2.0 are supported. For Sound Blaster setup, use port `220`, IRQ `7` and DMA `1`.

This application emulates a 286: Windows executables, 386/486 software and DOS/4GW remain unsupported. Hide the **Keys** panel to view **PC %** and **fps** at the bottom. These report guest timing and submitted images; physical game performance still needs testing. See the [DOS guide](../firmware/Tab5OS_Arduino_v0.8.53/DOS.md) for the FreeDOS image, current controls and remaining limits.

To rebuild the firmware, follow the [build guide](BUILD.md).

Official references: [esptool installation](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html), [writing an image](https://docs.espressif.com/projects/esptool/en/latest/esp32p4/esptool/basic-commands.html), [Tab5 download mode](https://docs.m5stack.com/en/uiflow2/Tab5/program).
