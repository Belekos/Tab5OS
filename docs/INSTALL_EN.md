# Install Tab5OS 0.8.53 on M5Stack Tab5

**Availability:** this source-only repository contains no firmware download or complete SD installation pack. The procedures below describe the verified 0.8.53 image formats for use once a legally distributable release is published. No M5Burner listing is claimed live. See [NOTICE.md](../NOTICE.md).

This build targets **M5Stack Tab5 with ESP32-P4 and 32 MiB PSRAM**, using the M5Stack `prev3` build variant, 16 MiB Flash and DIO mode. It is not an ESP32-S3/C6 image or a Linux distribution. The C6 companion firmware is neither included nor rewritten. Other hardware revisions still need validation; this qualification does not add a software compatibility lock.

## Select the right image

| Situation | File | Installation path |
| --- | --- | --- |
| Tab5OS is already installed; retain existing settings and data | `firmware/Tab5OS_0.8.53_OTA.bin` | Tab5OS built-in updater |
| First installation; no internal data must be retained | `firmware/Tab5OS_0.8.53_initial_install_16MB.bin` | Merged image at address `0x0` |

**The first-install image replaces the entire 16 MiB P4 Flash, including NVS, settings and internal data. Do not use it to update a personal installation.** It does not contain the microSD resources. Back up existing SD files before preparing the card; do not format a card whose contents must be retained.

For a firmware package, check its `SHA256SUMS.txt`; its `verify_package.py` validates image bytes without opening a device. These binary-package files are not part of this source-only repository.

## Update an existing Tab5OS installation

1. Copy `Tab5OS_0.8.53_OTA.bin` to a free location on the microSD card.
2. Select it in Explorer, then choose the firmware-install action from **Actions**.
3. Choose **Verify** and compare the displayed SHA-256 with `SHA256SUMS.txt`. The SDK's embedded technical description is not a reliable Tab5OS version label; the checksum identifies this release.
4. Choose **Install**, wait for success, then **Restart**. Keep power connected during the write.
5. Update the SD resources using the procedure below.

This path writes the application to the other OTA slot. It does not replace the partition table or NVS. Never write the OTA application image at address zero.

## First installation through M5Burner

Use a M5Burner version that supports **Tab5 / ESP32-P4** and the Tab5OS listing published by the project owner. Publication and successful import by M5Burner remain to be checked in the service; no physical flash has been performed while preparing this package.

1. Prepare the microSD card. For a new FAT32 card, copy the **contents** of the matching release's `sdcard` directory to the root of the card, not the ZIP file itself.
2. Connect the Tab5 USB-C port with a data cable. Hold Reset for about two seconds until the green LED flashes rapidly, then release it to enter download mode.
3. Select **Tab5OS 0.8.53**, device Tab5 and the correct serial port in M5Burner. The merged first-install image is written at `0x0`; its P4 bootloader is already embedded at `0x2000`.
4. Start only after accepting that the internal Flash contents will be replaced. No additional full-chip erase step is required by this guide.
5. After the transfer succeeds, restart with the prepared SD card inserted. Configure language and Wi-Fi inside Tab5OS. The distributed image contains no personal Wi-Fi credentials.

If the M5Burner listing is not public yet, these files are publication materials, not evidence that a listing is online. Its GitHub field must point to the actual repository before publishing.

## SD resources

Use resources from the **same 0.8.53 release**: language packs, fonts, keyboards, Python modules, external applications and the local city index. These are distributed separately from the firmware. Essential native applications remain accessible without a card, but features requiring external files are limited.

For a previously used card, **do not overwrite the whole `/System` directory**. Back up customizations, copy `/Factory/Tab5OS-0.8.53`, update the repair modules listed in [`LISEZ_MOI.md`](../firmware/LISEZ_MOI.md), then open `/MicroPython/update_sd.py` and use the normal repair/update action. The separate restore-modified-files action deliberately replaces custom resources. Follow the full repository instructions for that migration.

## Validation scope

Both images come from the verified ESP32-P4 build of 0.8.53. Checks cover application integrity, merged-image components, offsets, partition-table MD5 and initial OTA-selection CRC. NVS, app1, SPIFFS and coredump are filled with `0xFF` in this factory image; it was not exported from a personal tablet.

M5Burner import and physical boot have not been tested. See the project's verification report and [`TESTS_TAB5_0.8.53.md`](../firmware/TESTS_TAB5_0.8.53.md) before claiming hardware validation.

Official references: [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro), [Tab5 download mode](https://docs.m5stack.com/en/uiflow2/Tab5/program), [publishing](https://docs.m5stack.com/en/uiflow/m5burner/publish).


Partition table: the effective build table is the SDK’s `tools/partitions/default_16MB.csv`; the sketch does not contain `partitions.csv`. See [build details](BUILD.md).
