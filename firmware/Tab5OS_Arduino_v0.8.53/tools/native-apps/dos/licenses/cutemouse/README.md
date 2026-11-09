# CuteMouse driver disk

The read-only `/System/DOS/DRIVERS.IMG` is mounted as DOS drive B:.
It contains the unchanged CuteMouse 2.1b4 binary, matching source, GPL v2
licence and ASMLIB LGPL notice from the official FreeDOS 1.4 release.
The full source archive includes its MAKEFILE and assembly sources.
Upstream: https://cutemouse.sourceforge.net/ and https://github.com/FDOS/mouse

This separate disk leaves FREEDOS.IMG and the `/DOS` program directory intact,
so existing floppy overlays retain their original identity. The frontend runs
`B:\CTMOUSE /S14 /R11` before launching the program. The emulated serial mouse
uses COM1 and IRQ4; normal DOS INT33 functions are supplied by this real driver.
No Microsoft mouse-driver binaries are included.

Reproduce with `python prepare_drivers.py --cache-dir downloads --output
DRIVERS.IMG --licenses-dir cutemouse --offline` from the DOS source directory.
Input release SHA-256 values are shared with `prepare_freedos.py`; component
and generated image hashes are recorded in provenance.json.
