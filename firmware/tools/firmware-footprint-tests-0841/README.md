# Firmware footprint regression (0.8.41)

`python3 tools/firmware-footprint-tests-0841/run.py` builds the production helper
with ASan/UBSan and SDK boundary mocks, then runs 15 independent boot scenarios.
It checks valid images, missing partitions, truncated/overflowed partition bounds,
header read errors, bad magic/segment counts, SDK failures, wrong image start,
zero/oversized images, and exact boundary sizes. Each boot then makes 100,000
additional accesses: both success and error are cached, with no further I/O.
The metadata mock does not reimplement or certify the SDK's parser.

A separate target compile uses the actual M5Stack 3.3.9 / ESP-IDF 5.5.4 headers
and checks undefined symbols: there must be no `esp_image_verify`, hashing,
allocation, or `ESP.getSketchSize` dependency. The full release build checks link
integration. This is not a test on physical hardware.

The pinned upstream `esp_image_format.c` was independently reviewed:
`esp_image_get_metadata` supplies `do_load=false`, null SHA handle and null
checksum to segment processing; `process_segment_data` immediately returns
before mapping payloads for that combination. Checksum bytes are skipped;
image/segment headers and an optional 32-byte digest are read. This API is for
size display only, and does not replace bootloader/OTA integrity verification.
