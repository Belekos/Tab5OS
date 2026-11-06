# SD cards: capacity, format and memory

The storage mechanisms described here are retained in 0.8.21, with M5Stack package 3.3.9 and ESP-IDF 5.5.4. The structure measurements below come from the initial 0.8.5 audit. System-resource recovery and the fallback font are described in `REPARATION_SD.md`.

**A card larger than 32 GB does not necessarily require a different file system.** For this version, a FAT32 partition on a 64, 128 or 256 GB card remains the practical choice. The current driver can address these capacities; each card model still needs testing on the tablet. This release does not format cards or add exFAT support.

## Two different limits

| Item | Limit or behavior |
| --- | --- |
| Card capacity | 32 GB is the boundary between the SDHC and SDXC standards; it is not FAT32's maximum capacity. |
| Individual FAT32 file | At most 4,294,967,295 bytes, or 4 GiB minus one byte. |
| New SDXC cards | Usually formatted as exFAT, which the supplied firmware cannot mount. An unrecognized card is therefore not necessarily too large. |
| Tab5OS Wikipedia library | Archives split into 64 MiB segments, with a 64-bit total size and logical offsets. The complete encyclopedia can exceed 4 GiB. |
| Map tiles | Separate PNGs or packages of 1,900 MiB by default (2,000 MiB maximum), because SDK read offsets are signed 32-bit values. Up to 128 packages are recognized on the SD card. |

The SPI driver in this build uses 32-bit sector numbers and 512-byte sectors. It has no 32 GB limit; its addressable range is around 2 TiB. This does not certify 2 TB cards: the sector-count calculation itself reaches its limit at 2^32 sectors. SDUC cards above 2 TB are not a target of this version.

The Espressif page linked in the discussion is evolving documentation; its `stable` branch may differ from the SDK compiled here. Its table includes a theoretical FAT32 limit that is not the limit of the SPI driver compiled here. The headers and libraries actually used by the firmware were also examined for this analysis.

## The format does not free application RAM

Neither the card contents nor the entire allocation table are loaded into RAM. Moving from 32 to 128 GB therefore does not quadruple the memory reserved for the file system. A larger card can, however, make some free-space searches or operations on densely populated directories take longer.

Compiled settings and structure sizes checked with the firmware's RISC-V compiler:

| Setting or structure | Observed value | Consequence |
| --- | --- | --- |
| `FF_FS_EXFAT` | `0` | exFAT disabled. |
| `FF_LBA64` | `0` | 32-bit sector addressing. |
| `FSIZE_t`, `off_t`, `size_t` | 4 bytes each | Large files would also require an API audit, not just enabling exFAT. `off_t` is signed. |
| Private per-file cache | Enabled | Files have their own sector cache. |
| Configured maximum sector size | 4,096 bytes | Caches are sized for 4 KiB even when the card uses 512-byte sectors. |
| `sizeof(FIL)` | 4,136 bytes | Cost of a file slot in the FatFs mount. |
| `sizeof(FATFS)` | 4,152 bytes | Volume structure, excluding other VFS-context elements. |
| Maximum files per mount | 24 | 99,264 bytes, approximately 97 KiB, reserved for these slots at mount time. |
| FatFs allocation | PSRAM preferred, SRAM fallback | These reservations normally use available PSRAM. |
| Long filenames | 512-byte stack buffer | Temporary cost to account for in tasks that access files. |

The 24 slots are reserved at mount time, regardless of the number of files currently open. Closing an application closes its files and frees its own buffers, but does not destroy the card's shared context. This reservation is therefore not a cache leak from the previous application.

Arbitrarily reducing the 24 slots would mainly save a few tens of KiB of PSRAM, while risking exhaustion of open-file slots when several features are active. It does not resolve SRAM shortages in decoders, task stacks or DMA transfers.

A controlled SDK rebuild could enable caches sized for the actual sector size (`CONFIG_FATFS_USE_DYN_BUFFERS`), currently disabled. This is a more relevant optimization than changing the card format, but cannot be enabled correctly by a simple `#define` in the application when FatFs is already precompiled.

## What exFAT and LittleFS would offer

**exFAT** would be useful for copying very large individual files directly and retaining PC compatibility. It is not a remedy for SRAM consumption. Enabling it changes FatFs structures, file sizes and offsets; changing only a header without rebuilding the corresponding libraries would be incorrect. It would require rebuilding the compatible storage stack, then adapting and verifying Arduino, POSIX, media-player and Python code paths that still use 32-bit offsets. Segmented archives already support large Wikipedia libraries without this migration.

**LittleFS** offers benefits for power-loss resilience and cache control. It is inconvenient for removable media that must be populated directly from Windows: the PC does not mount it natively like FAT32 or exFAT. Switching to it would not fix a loss of SPI communication, unstable power or concurrent unmounting. It is therefore not the selected format for the card holding documents, maps and encyclopedias.

## Recommended use for this version

Keep FAT32, including on cards larger than 32 GB. To prepare a new card, back up its data first, create a primary FAT32 partition using an MBR partition table, then copy the Tab5OS SD resources back onto it. The formatting tool must actually offer FAT32 across the full capacity: the capabilities of the Windows interface and its format command depend on the Windows version. Simply choosing an SDXC tool's default format may produce exFAT.

Wi-Fi sharing in this version also uses signed offsets: transfers are limited to 2 GiB minus one byte per file. The 64 MiB Wikipedia segments and new map volumes remain compatible.

Do not change the format of a working card solely to save RAM. For Wikipedia archives obtained on a PC, use the project's import or splitting tools; copying a ZIM of several tens of GB as one file onto FAT32 is not possible.

The SPI clock in this version is fixed at 10 MHz. Downloads retry transient errors. A Python program can temporarily release its storage access to let the interface remount the SD card; this requires closing all its files and directory iterators. A network fault has a different error code from an SD error.

Document backup copies are stored in `/.tab5-backups`. Explorer hides names beginning with a dot and `.bak` files; Ctrl+H or the unobtrusive show-hidden-items control makes them visible. The eye icon hides or shows a selection without renaming it; this Tab5OS preference does not change Windows attributes.

Tab5OS automatic recovery remains bounded and waits for card users to close before remounting. It cannot repair FAT structures damaged by a power loss during a write. In that case, back up readable files and check the card on a PC before resuming downloads.

## Sources and provenance

- [Espressif: file-system choices, version 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-guides/file-system-considerations.html).
- [Espressif: the same guide, stable branch](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-guides/file-system-considerations.html).
- [FatFs: limits and memory usage](https://elm-chan.org/fsw/ff/doc/appnote.html), [configuration options](https://elm-chan.org/fsw/ff/doc/config.html).
- [SD Association: SD, SDHC, SDXC and SDUC capacity standards](https://www.sdcard.org/developers/sd-standard-overview/capacity-sd-sdhc-sdxc-sduc/).
- [M5Stack: using Tab5 microSD with Arduino](https://docs.m5stack.com/en/arduino/m5tab5/microsd).
- [Espressif: Arduino as an ESP-IDF component and rebuilding the core](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html).
- Local audit: `sdkconfig`, `ffconf.h`, `ff.h`, `SD.cpp` and `sd_diskio.cpp` from the M5Stack 3.3.9 package used, plus [`vfs_fat.c`](https://github.com/espressif/esp-idf/blob/v5.5.4/components/fatfs/vfs/vfs_fat.c) and [`ffsystem.c`](https://github.com/espressif/esp-idf/blob/v5.5.4/components/fatfs/port/freertos/ffsystem.c). The structure sizes above were verified by compilation for this project's ESP32-P4 target, not inferred from a PC build.
