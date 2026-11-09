# Component licenses in Tab5OS 0.8.53

Third-party component notices are retained in their directories. [LICENCES-manifest.json](LICENCES-manifest.json) lists their paths and hashes. This inventory describes the supplied files; it does not assign a new license to their authors.

**No main license file has been found for the parts specific to Tab5OS.** The rights holder must choose redistribution terms before describing the repository as an open-source project under a particular license. Preparing the repository and its documentation does not make that choice.

## Components

| Scope | Retained notices |
| --- | --- |
| Firmware, RTL-SDR driver | GPL sources with per-file notices: retain their source code and provenance, and examine the license of the linked firmware |
| ELF loader and Espressif USB drivers | Apache-2.0, with license texts retained; do not relabel them MIT |
| MicroPython and Draftling Markdown parser | MIT, with notices retained |
| libssh2 client | BSD-3-Clause, with notice and provenance in `src/tab5ssh` |
| libgit2 | GPL-2.0 with an explicit upstream linking exception; retain `COPYING`, `AUTHORS`, `UPSTREAM.json` and per-file notices in `src/tab5git` |
| WebP/Zstandard/XZ decoders, JPEG encoder, stb | Separate licenses listed in the manifest and upstream files |
| GB/GBC/NES/Sega8/GBA/Doom ELF applications | Multiple MIT/GPL licenses depending on the core; adapters, sources and notices remain supplied with each application |
| DOS emulator / esp32pc and esp32emu | MIT, with pinned upstream sources and both original license files retained |
| DOS OPL2 sound synthesis / emu8950 | Mitsutaka Okazaki's MIT license is retained in `tools/native-apps/dos/core/emu8950/LICENSE` |
| FreeDOS guest boot disk | Kernel, FreeCOM, KEYB and keyboard layouts keep their original GPL and library notices. Corresponding source archives and build documentation are supplied in `tools/native-apps/dos/licenses/freedos` and in the SD ZIP notices directory |
| CuteMouse DOS mouse driver | The separate read-only driver disk contains the official FreeDOS CuteMouse 2.1b4 executable, corresponding assembly sources, GPL-2.0 license and ASMLIB LGPL-2.1 notice. The same notices and sources are retained in `tools/native-apps/dos/licenses/cutemouse` |
| MegaDrive / Gwenesis | AGPL-3.0 in the core directory; a separate ELF application, without reclassification as a permissive license |
| SNES / ndssfc / Snes9x | Mixed notices: GPL for some parts and the Snes9x **non-commercial / personal-use** restriction. This component cannot be described as entirely under a permissive open-source license |
| Main DejaVu Sans Mono font | Bitstream Vera license and DejaVu modifications, present in the TTF metadata and reproduced in the manifest |
| CJK fonts and input dictionaries | OFL-1.1, Apache-2.0 and GPL depending on the resource; notices and dictionary sources in `sdcard/System/ime` |

External Arduino library files remain those of the versions documented in [BIBLIOTHEQUES.md](BIBLIOTHEQUES.md). Compilation does not make them project-owned files. License texts in the sources, including those specific to individual files, remain authoritative; a table label does not replace them.

Component license texts remain authoritative for their use and redistribution.

The DOS guest image is assembled from the pinned official FreeDOS 1.4 packages.
It contains no Microsoft MS-DOS binaries. `tools/native-apps/dos/prepare_freedos.py`
reproduces the supplied FAT12 image and verifies the upstream input archives and
extracted binaries. Keep its corresponding source archives, provenance and
license texts with any redistributed image. In the separate SD ZIP these files
are included under `notices/`, alongside `LICENCES-manifest.json`; the complete
package also retains them with the emulator sources.

The additional `/System/DOS/DRIVERS.IMG` disk is produced separately by
`prepare_drivers.py`. Its provenance pins the CuteMouse executable and source
archive and records the generated image hash. Its files do not alter the existing
FreeDOS boot image or saved floppy sectors.
