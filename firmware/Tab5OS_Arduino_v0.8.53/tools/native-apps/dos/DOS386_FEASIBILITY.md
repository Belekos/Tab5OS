# 386/486 feasibility investigation

This is a development note. The shipped DOS application still uses the 80286
core; this investigation does **not** enable 386/486 programs in that application.

## Result

A separate 386-class engine is technically plausible on the ESP32-P4. It is
not a configuration switch in the existing 286 core, and a newer emulated CPU
does not automatically make a game faster.

The candidate is [Tiny386](https://github.com/hchunhui/tiny386), inspected at
commit `a080c5d347f432481710902423c93992edde52d7`. It is written in C, has
386 protected mode and paging, and implements selected later instructions.
Its `gen = 3/4` setting is useful for 386/486 identification; it is not a claim
of complete 486 conformance or physical 486 speed. Upstream documents remaining
limitations in hardware task switching, debugging and permission checks.

## What was verified

An isolated Windows host prototype was compiled from the candidate's CPU and
PC devices with MSVC C11. It uses the upstream switch interpreter; the GNU
computed-goto and optional predecoder paths were not exercised by this build.

- A CPU test entered 32-bit protected mode and verified arithmetic, 32-bit
  storage, and the 486 `BSWAP`, `XADD` and `CMPXCHG` instructions: six checks.
- The complete PC prototype booted the real Tab5OS FreeDOS kernel and FreeCOM.
- FreeDOS loaded an original `.COM` that used 386 32-bit instructions and an
  original MZ `.EXE` that checked the three 486 instructions. Both printed their
  success markers to the emulated VGA screen. The harness checked that screen.
- An additional original `.COM` loaded its own GDT and switched from DOS real
  mode to 32-bit protected mode. It checked `BSWAP`, `XADD`, `CMPXCHG`, distinct
  low/high addresses across the A20 boundary, and storage above 1 MiB. It then
  switched through a 16-bit selector back to real mode, restored SS:SP and
  interrupt flags, printed its success marker, and terminated through DOS.
  FreeCOM executed the following batch line, proving return to the shell.
- The test used 8 MiB guest RAM, 1 MiB VGA memory and a 720 x 480 RGB565 canvas.

These are host tests. No RISC-V application, physical Tab5 performance result,
DOS/4GW or other DOS-extender test, DPMI/paging test, mouse test or sound test was
produced for this candidate. The first protected-mode CPU check runs directly;
the COM and MZ instruction checks run under DOS in real mode with 32-bit
instruction prefixes. The new COM transition probe exercises protected mode
from DOS, but does not establish compatibility with arbitrary 32-bit programs.

### Reproduce the complete DOS probe

Original probe sources are in `tests/tiny386_probe/`. Use an x64 Visual Studio
Developer Command Prompt with Python 3 and a clean Tiny386 checkout at the
commit above. Supply the exact private feasibility BIOS pair described below;
the harness verifies their SHA-256 values. It neither downloads nor distributes
the ROMs and never edits the input FreeDOS image or Tiny386 checkout.

```text
python tests/tiny386_probe/run_probe.py --tiny386 C:/path/to/tiny386 --bios C:/path/to/bios.bin --vgabios C:/path/to/vgabios.bin --image ../../../sdcard/DOS/FREEDOS.IMG --output C:/scratch/new-probe-directory
```

Run that command from `tools/native-apps/dos`. The output directory must be new.
The runner records its compiler log, captured boot output and `report.json`.
Success requires all three program markers and the batch line after their DOS
termination. The committed probe was rebuilt and rerun successfully on Windows.
Its generated code and images are test artifacts, not a native DOS application.

The test BIOS binaries came from the
[existing P4 port](https://github.com/winitboonkliang/windows95-esp32p4), commit
`022d1879d788bb7410a91c973badb3e14d083834`, and identify themselves as
SeaBIOS/SeaVGABIOS `rel-1.17.0-13-gc2a33ad9-dirty`. They were used only for this
private feasibility test. A release must rebuild the BIOS from an exact source
commit with the matching Tiny386 floppy patch and include the corresponding
source and licence material.

## Work required before an optional native application

1. Use a separate application and a fixed allocation budget. A starting design
   of 8 MiB guest RAM, 1 MiB VGA RAM and a 2 MiB decoded-block cache is plausible,
   but its total allocation including code, stacks, UI, disk state and the OS
   must be measured. The predecoder also allocates lookup tables.
2. Adapt block devices to the existing SDK directory projection and bounded
   floppy copy-on-write storage. Upstream stdio opens disks for direct writes;
   that behavior must not be inherited by the Tab5OS adapter. Preserve failed
   reads/writes and close every descriptor on normal exit and allocation failure.
3. Audit physical memory boundaries. Normal load/store helpers and the fast
   translation cache already have address checks, and DMA has range checks.
   However, page-table traversal directly reads CR3/PDE-derived addresses and
   writes accessed/dirty bits. Validate complete 4-byte entries before these
   accesses. Also test accesses straddling the final RAM byte and page boundary,
   arithmetic overflow, malformed descriptors, and invalid DMA lengths.
4. Validate the optimized CPU paths on RV32. Use aligned-safe loads where
   required, bound and invalidate predecoded blocks after guest code writes,
   and retain an interpreter fallback. Test allocation failure at each stage.
5. Connect timing, PS/2 input and PCM output to the cooperative SDK loop. Keep
   video timing running when display frames are skipped. Add explicit volume
   controls and measure CPU, disk, video and audio costs separately.
6. Pass FreeDOS COM/MZ, 32-bit DOS extender, paging, input, sound, storage and
   shutdown tests using the exact target ELF before offering it as experimental.

Tiny386's own code uses BSD-3-Clause; several peripheral files retain MIT-style
notices. SeaBIOS has its separate LGPL terms. Optional FMOPL code has LGPL-2.1-or-
later terms. Keep each component's notices and source obligations separate.

The external P4 port reports approximately 1.5 million interpreted instructions
per second after optimization. That is another board and firmware, not a Tab5OS
measurement. It supports investigating this route; it does not establish that
Monkey Island or arbitrary 386/486 games will be playable here.
