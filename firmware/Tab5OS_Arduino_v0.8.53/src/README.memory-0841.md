# Tab5OS memory layout

The vendored algorithms and licenses remain unchanged. These local changes keep
immutable lookup data in flash rather than internal SRAM:

- `tab5git/src/util/hash/sha1dc/ubc_check.{c,h}`: make `sha1_dvs` const in both
  declaration and definition. All uses in `sha1.c` only read the table. Collision
  detection and safe hashing stay enabled.
- `xz/xz_crc32.c` and `xz/xz_crc64.c`: replace runtime table construction with
  const tables in the adjacent generated headers, using exactly the original
  reflected polynomials (0xEDB88320 and 0xC96C5795D7870F42). The public init
  functions remain as no-ops for source compatibility. This removes 3,072 BSS
  bytes. There are no allocator calls or new initialization failure paths.

The tables remain in flash read-only sections. No TCM, LP RAM or task stack is
relocated by these changes.

## Physical banks and allocator figures

`Tab5MemoryLayout` distinguishes the 768 KiB HP L2MEM bank, the configured
256 KiB L2 cache, the 8 KiB SPM bank (called TCM in the heap API), the 32 KiB
LP SRAM bank, and external PSRAM. TCM and SPM are two names for the same bank;
they must not be added together. The allocator's total is the registered heap
capacity after code, static data, cache and reserved memory. It is not the
physical chip capacity.

On the pinned ESP32-P4 SDK, HP heaps have `INTERNAL | DMA | 8BIT`, while TCM
and LP heaps do not have DMA capability. Queries therefore identify each
sub-pool directly; they do not subtract unrelated largest-free-block values.
`INTERNAL | 8BIT` is the aggregate and must not be added to its three parts.
Default periodic samples use counters only. Largest-free-block scans are
requested only for explicit technical diagnostics. Pool samples are sequential,
not an atomic cross-pool snapshot.

## Firmware size display

`Tab5FirmwareFootprint::snapshot()` caches the running image and partition size
once per boot. It uses the official `esp_image_get_metadata` API after bounded
header validation; it does not call `ESP.getSketchSize()` or
`esp_image_verify()` on each system-info refresh. In the pinned SDK, metadata
mode skips segment payload mapping, checksum processing and SHA verification.
Only image/segment headers and the optional appended 32-byte digest are read.
Errors are also cached so a failed metadata read cannot cause repeated I/O.
The UI marks the size unavailable on failure. Bootloader/OTA integrity checking
is unchanged; this helper is only an informational size query.

