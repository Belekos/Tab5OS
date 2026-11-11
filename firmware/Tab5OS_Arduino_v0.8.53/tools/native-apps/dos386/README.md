# DOS x86 for Tab5OS

This is a separate native application for broader DOS compatibility. The
existing 286 application remains available. Monkey Island is one possible
workload, not a compatibility target or a special case in this implementation.

## Current status

Source integration and local host tests are complete. The RISC-V application
and reproducibly rebuilt ROM pair have **not yet been compiled or executed**.
Do not present this source checkpoint as an installable or hardware-tested
release. The validation workflow builds the exact application, then runs it
through the ABI7 harness before any application package can be offered.

## Configuration

- Tiny386 pinned to `a080c5d347f432481710902423c93992edde52d7`.
- 32-bit x86 core, generation 6; real/protected mode and paging.
- x87, MMX, SSE, SSE2, SSE3 and SSSE3 compiled in; instruction probes exercise
  selected operations, not every instruction or all architectural behavior.
- Fast interpreter with a 1 MiB predecoded-block cache; aligned-safe RV32 access.
- 8 MiB guest RAM and 1 MiB VGA RAM inside a 16 MiB application arena.
- VGA/VBE, up to a 1024 x 768 source framebuffer; 4:3 graphics presentation.
- PS/2 keyboard and mouse, AdLib, PC speaker and Sound Blaster 16.
- Same-worker emulation and audio mixing, bounded cooperative CPU batches,
  volume/mute controls, MIPS/FPS and application-memory diagnostics.

There is no fixed 6 MHz emulation target. Instructions run as fast as the host
budget permits while device clocks follow host time. MIPS is a measured guest
instruction count, not a claim of equivalent physical-PC clock speed.

## Compatibility limits

The x87 implementation is double-based, without complete 80-bit precision or
all x87 exceptions. Upstream also documents incomplete hardware task switching,
debugging and privilege checks. Disabled-A20 address wrapping is not implemented
by the upstream CPU; enabled-A20 protected-mode tests do not verify that legacy
behavior. x86-64 is not enabled. Windows PE/NE launch is
not supported. DOS MZ executables containing LE/LX extender payloads are allowed.
There is no guest network interface. Features do not imply real-time speed on
the Tab5: physical hardware measurements remain necessary.

The selected SD directory becomes a read-only FAT16 C: drive. The FreeDOS A:
image supports bounded sector changes and explicit Save/normal Quit persistence.
Original SD images and files are never opened writable. Saved overlays use the
existing ABI7 `/Saves/DOS` allowance with a separate x86 identity salt; the old
286 save identity is unchanged. The total overlay capacity is 3072 sectors.

## Build and verify

On Linux, with the pinned Espressif GCC toolchain:

```sh
python3 prepare_bios.py --output /tmp/tab5-x86-bios
python3 tests/run_cpu_tests.py --output /tmp/tab5-x86-cpu --sanitize
python3 tests/run_devices.py --output /tmp/tab5-x86-devices --sanitize
python3 tests/run_frontend_tests.py --output /tmp/tab5-x86-ui
python3 tests/extender/prepare.py --output /tmp/tab5-x86-extender
python3 build.py --toolchain /path/to/riscv32-esp-elf/bin -o '/tmp/DOS x86.app.elf'
python3 tests/emulate_app.py --elf '/tmp/DOS x86.app.elf' \
  --image ../../../sdcard/DOS/FREEDOS.IMG --bios-dir /tmp/tab5-x86-bios \
  --extender /tmp/tab5-x86-extender/TESTDPMI.EXE --output /tmp/tab5-x86-app.json
```

The host checks cover memory boundaries, paging, self-modifying code, peripheral
IO, UI layout and audio ownership. The exact-ELF harness checks real FreeDOS,
COM/MZ programs, a real CWSDPMI extender, VGA pixels, keyboard, PS/2 mouse,
speaker/mute, immutable bases, persisted COW contents and shutdown. It does not
replace a physical ESP32-P4 test.

See `UPSTREAM.txt`, `core/LICENSE`, retained device notices and `bios/NOTICE.txt`.
The ROM builder includes complete patched SeaBIOS sources and its licences in
its output. Third-party extender fixtures are test inputs only.
