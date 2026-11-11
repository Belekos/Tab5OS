# Actual 32-bit DOS extender integration probe

`prepare.py` downloads and verifies the official DJGPP archive copies of
CWSDPMI release 7, generates an original 32-bit COFF client, and appends it to
the **unmodified** `CWSDSTUB.EXE`. The resulting `TESTDPMI.EXE` is self-contained:
FreeDOS starts the actual third-party extender, which initializes protected
mode, paging and a ring-3 client before our code runs.

```text
python prepare.py --output /scratch/dos386-extender
```

Use `--cache /directory` to supply the two exact release archives offline.
Run `DPMI.COM`, which launches `TESTDPMI.EXE` with a temporary DOS critical-error
handler. This prevents a read-only C: drive from opening an interactive
Abort/Retry/Fail prompt when the unchanged extender attempts to create its
default swap file. The launcher restores the previous handler and prints
`TAB5 DPMI RETURN OK` only after the child returns exit status zero.

The generated program verifies the DPMI version, allocates 256 KiB, creates an
LDT descriptor, writes and checks all 64 pages, checks a dword spanning two
pages and the allocation's final dword, then releases both resources. It
prints `TAB5 DPMI32 OK` only on success and exits through DOS. The integration
harness must observe that rendered marker, the launcher's return marker and
subsequent shell activity.
A failure prints `TAB5 DPMI32 FAIL` and exits with status 1.

The unchanged loader defaults to `C:\CWSDPMI.SWP`. Use only the test harness's
isolated virtual disks; no user files are needed. An unavailable swap file may
produce a warning, but this probe fits physical guest RAM and does not require
swapping. The probe is not a DOS/4GW, Windows or arbitrary application test.

## Provenance and redistribution

CWSDPMI is Copyright (C) 1995-2010 Charles W Sandmann. Its release documentation
permits redistribution under the GPL with source, or redistribution of its
unchanged binaries under the stated conditions; it explicitly permits adding
a COFF image after `CWSDSTUB.EXE`. Users have the right to receive CWSDPMI
source code and binary updates. The generated fixture directory includes the
complete unmodified binary and source release archives, `CWSDPMI.DOC`,
`COPYING` and `COPYING.CWS`.

- Author: http://sandmann.dotster.com/cwsdpmi/
- Binary: https://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7b.zip
- Source: https://www.delorie.com/pub/djgpp/current/v2misc/csdpmi7s.zip
- COFF format: https://www.delorie.com/djgpp/doc/coff/
- DPMI API: https://www.delorie.com/djgpp/doc/dpmi/

The original `program.py`, `wrapper.py` and their generated clients are dedicated to the
public domain under CC0-1.0. The official extender stub retains its own terms.
Fixture generation is reproducible and records SHA-256 values; this does not
claim that the historical third-party binary was rebuilt with a modern toolchain.
