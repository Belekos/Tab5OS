#!/usr/bin/env python3
"""Build a separate read-only driver floppy without changing user DOS snapshots."""
import argparse
import io
import json
from pathlib import Path
import struct
import zipfile
from prepare_freedos import FAT, checked, fetch, make_floppy, sha, write_file

EXPECTED = {
    'CTMOUSE.ZIP': 'dd378ddfe5b6948d3073c12acaf0ca31bf60d0898f1cd708b6858083a14a41c7',
    'CTMOUSE.EXE': '822cf550c9e19a22785722d2306aa08ede10ff20bfe931d5a81a15f77c5f363e',
    'ctmouse-source.zip': '3dd29f829496032d4440a0633ca2587b8ef9b452a6d6feb48d713010529c27d6',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--licenses-dir', type=Path, required=True)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    floppy = zipfile.ZipFile(io.BytesIO(fetch(args.cache_dir, 'FD14-FloppyEdition.zip', args.offline)))
    boot = floppy.read('144m/x86BOOT.img')
    usb = zipfile.ZipFile(io.BytesIO(fetch(args.cache_dir, 'FD14-LiteUSB.zip', args.offline))).read('FD14LITE.img')
    files = dict(FAT(usb[struct.unpack_from('<I', usb, 454)[0] * 512:]).files())
    package = checked(files['PACKAGES/BASE/CTMOUSE.ZIP'], EXPECTED['CTMOUSE.ZIP'], 'CTMOUSE.ZIP')
    z = zipfile.ZipFile(io.BytesIO(package))
    driver = checked(z.read('BIN/CTMOUSE.EXE'), EXPECTED['CTMOUSE.EXE'], 'CTMOUSE.EXE')
    source = checked(z.read('SOURCE/CTMOUSE/SOURCES.ZIP'), EXPECTED['ctmouse-source.zip'], 'ctmouse-source.zip')
    sources = zipfile.ZipFile(io.BytesIO(source))
    notice = (
        'Tab5OS read-only DOS driver disk B:\r\n'
        'CuteMouse 2.1b4 from the official FreeDOS 1.4 CTMOUSE package.\r\n'
        'Start with B:\\CTMOUSE /S14 /R11 (COM1, IRQ4, fixed sensitivity).\r\n'
        'GPL version 2; ASMLIB LGPL terms are included separately.\r\n'
        'Matching source and build files are in SOURCE.ZIP.\r\n'
        'Project: https://cutemouse.sourceforge.net/\r\n'
        'The original boot disk A: and its saved sectors remain unchanged.\r\n'
    ).encode('ascii')
    payload = {'CTMOUSE.EXE': driver, 'COPYGPL.TXT': sources.read('COPYING'),
               'ASMLGPL.TXT': sources.read('ASMLIB/LGPL'), 'SOURCE.ZIP': source,
               'README.TXT': notice}
    image = make_floppy(boot, payload)
    write_file(args.output, image, args.force)
    for name, raw in {'ctmouse-source.zip': source, 'LICENSE-ctmouse.txt': sources.read('COPYING'),
                      'LICENSE-asmlib.txt': sources.read('ASMLIB/LGPL'),
                      'ctmouse-package.lsm': z.read('APPINFO/CTMOUSE.LSM'),
                      'CTMOUSE.txt': z.read('DOC/CTMOUSE/CTMOUSE.TXT'),
                      'INT33.txt': z.read('DOC/CTMOUSE/INT33.LST')}.items():
        write_file(args.licenses_dir / name, raw)
    readme = '''# CuteMouse driver disk

The read-only `/System/DOS/DRIVERS.IMG` is mounted as DOS drive B:.
It contains the unchanged CuteMouse 2.1b4 binary, matching source, GPL v2
licence and ASMLIB LGPL notice from the official FreeDOS 1.4 release.
The full source archive includes its MAKEFILE and assembly sources.
Upstream: https://cutemouse.sourceforge.net/ and https://github.com/FDOS/mouse

This separate disk leaves FREEDOS.IMG and the `/DOS` program directory intact,
so existing floppy overlays retain their original identity. The frontend runs
`B:\\CTMOUSE /S14 /R11` before launching the program. The emulated serial mouse
uses COM1 and IRQ4; normal DOS INT33 functions are supplied by this real driver.
No Microsoft mouse-driver binaries are included.

Reproduce with `python prepare_drivers.py --cache-dir downloads --output
DRIVERS.IMG --licenses-dir cutemouse --offline` from the DOS source directory.
Input release SHA-256 values are shared with `prepare_freedos.py`; component
and generated image hashes are recorded in provenance.json.
'''
    write_file(args.licenses_dir / 'README.md', readme.encode('utf8'))
    provenance = {'component': 'CuteMouse 2.1b4, official FreeDOS 1.4 package',
                  'source_package': 'PACKAGES/BASE/CTMOUSE.ZIP',
                  'source_release': 'https://download.freedos.org/1.4/FD14-LiteUSB.zip',
                  'expected': EXPECTED,
                  'image': {'size': len(image), 'sha256': sha(image)},
                  'files': {n: {'size': len(v), 'sha256': sha(v)} for n, v in payload.items()}}
    write_file(args.licenses_dir / 'provenance.json', (json.dumps(provenance, indent=2) + '\n').encode('utf8'))
    print(json.dumps(provenance['image']))


if __name__ == '__main__':
    main()
