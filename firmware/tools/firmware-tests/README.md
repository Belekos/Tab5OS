# Firmware installation regression checks

Run `run_msvc.cmd <sketch-directory> --report <report.json>` on Windows with
Visual Studio C++ installed. The runner copies `Tab5Firmware.cpp` byte for byte,
then compiles and executes it against deterministic file, task and OTA stubs.

Checks cover direct installation without preliminary inspection, reviewed file
installation, size/hash changes after inspection, equivalent FAT path casing,
another selected file, explicit review clearing, invalid and missing paths,
wrong chip/revision, truncated and oversized images, OTA failures, cancellation,
storage/task contention, and the preservation of USB paths.

The digest stub only exercises change detection branches. This does not validate
SHA-256, ESP image parsing, real flash writes, or electrical USB removal; the P4
build and testing on a physical device are separate checks. No device is flashed.
