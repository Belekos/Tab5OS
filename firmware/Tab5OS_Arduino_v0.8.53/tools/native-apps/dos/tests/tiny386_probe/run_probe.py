"""Reproduce the isolated host 386/486 probe; no target application is built.

Run from an x64 Visual Studio developer shell. The Tiny386 checkout and the
private feasibility BIOS binaries are explicit inputs, never downloaded here.
Only original probe sources are distributed with Tab5OS; generated executables
and guest images belong in a scratch output directory.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

from programs import protected_program

PIN = 'a080c5d347f432481710902423c93992edde52d7'
BIOS_SHA = 'ec4bacb00b7176111ddcef8a1da5900725e1bcd9cd49c0fa053434de385fb522'
VGA_SHA = 'f5156868d2634b6fd34d68f640b14f37431ffb02810bebfbe8a1fbc99ebdfaa6'
HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def instruction_program(gen, base):
    # Original small DOS programs, CC0. COM uses base 100h; MZ image uses 0.
    code = bytearray(b'\x0e\x1f')
    branches, fixups, labels = [], [], {}
    def compare(value):
        code.extend(b'\x66\x3d' + struct.pack('<I', value))
        code.append(0x75); branches.append(len(code)); code.append(0)
    if gen == 3:
        code.extend(bytes.fromhex('66 b8 78 56 34 12 66 b9 00 00 01 00 66 01 c8 66 c1 c0 08'))
        compare(0x35567812)
    else:
        code.extend(bytes.fromhex('66 b8 78 56 34 12 66 0f c8 66 bb 02 00 00 00 66 0f c1 d8'))
        compare(0x78563414)
        code.extend(bytes.fromhex('66 89 c1 66 ba ef be ad de 66 0f b1 d1 66 89 c8'))
        compare(0xdeadbeef)
    def output(label, status):
        code.append(0xba); fixups.append((len(code), label)); code.extend(b'\0\0')
        code.extend(b'\xb4\x09\xcd\x21\xb8' + bytes([status, 0x4c]) + b'\xcd\x21')
    output('success', 0)
    fail = len(code); output('failed', 1)
    labels['success'] = len(code); code.extend(f'TAB5 {gen}86 DOS OK\r\n$'.encode())
    labels['failed'] = len(code); code.extend(f'TAB5 {gen}86 DOS FAIL\r\n$'.encode())
    for pos in branches:
        delta = fail-pos-1
        if not 0 <= delta < 128: raise ValueError('Short branch overflow')
        code[pos] = delta
    for pos, label in fixups: struct.pack_into('<H', code, pos, base+labels[label])
    return bytes(code)


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tiny386', type=Path, required=True)
    parser.add_argument('--bios', type=Path, required=True)
    parser.add_argument('--vgabios', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True, help='Original Tab5OS FreeDOS floppy')
    parser.add_argument('--output', type=Path, required=True, help='New scratch output directory')
    args = parser.parse_args()
    for key in ('tiny386', 'bios', 'vgabios', 'image', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    if shutil.which('cl') is None:
        parser.error('cl not found; run from an x64 Visual Studio developer shell')
    revision = subprocess.check_output(['git', '-C', str(args.tiny386), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != PIN: parser.error('Tiny386 checkout must be pinned to '+PIN)
    tracked = subprocess.check_output(['git', '-C', str(args.tiny386), 'status', '--porcelain', '--untracked-files=no'], text=True)
    if tracked.strip(): parser.error('Tiny386 tracked sources must be clean')
    if digest(args.bios) != BIOS_SHA or digest(args.vgabios) != VGA_SHA:
        parser.error('This probe requires the exact documented private feasibility BIOS pair')
    args.output.mkdir(parents=True, exist_ok=False)
    portable = args.output/'portable'; portable.mkdir()
    for name in ('pci.c', 'vga.c', 'adlib.c', 'i386.c'):
        source = (args.tiny386/name).read_text()
        source = re.sub(r'case (0x[0-9a-f]+) \.\.\. (0x[0-9a-f]+):',
            lambda m: ' '.join(f'case 0x{x:x}:' for x in range(int(m[1],16), int(m[2],16)+1)), source)
        source = source.replace('struct AdlibState {\n};', 'struct AdlibState { int unused;\n};')
        if name == 'i386.c':
            source = source.replace('res < -2147483648', 'res < -INT64_C(2147483648)')
        (portable/name).write_text(source)
    spec = importlib.util.spec_from_file_location('fd', HERE.parents[1]/'prepare_freedos.py')
    fd = importlib.util.module_from_spec(spec); spec.loader.exec_module(fd)
    original = args.image.read_bytes(); files = dict(fd.FAT(original).files())
    files['TEST386.COM'] = instruction_program(3, 0x100)
    body = instruction_program(4, 0); size = 32+len(body)
    files['TEST486.EXE'] = struct.pack('<14H', 0x5a4d, size%512, (size+511)//512, 0, 2, 32,
        0xffff, 0, 0xfffe, 0, 0, 0, 0x1c, 0)+b'\0'*4+body
    files['TESTPM.COM'] = protected_program()
    files['AUTOEXEC.BAT'] = b'@ECHO OFF\r\nTEST386.COM\r\nTEST486.EXE\r\nTESTPM.COM\r\nECHO TAB5 PROTOTYPE DONE\r\n'
    files['FDAUTO.BAT'] = files['AUTOEXEC.BAT']
    image = args.output/'FREEDOS-probe.IMG'; image.write_bytes(fd.make_floppy(original, files))
    for name in ('TEST386.COM', 'TEST486.EXE', 'TESTPM.COM'): (args.output/name).write_bytes(files[name])
    core_names = ('i8042.c', 'i8254.c', 'i8259.c', 'i8257.c', 'ide.c', 'ini.c', 'pc.c',
                  'misc.c', 'vgafont.c', 'sb16.c', 'pcspk.c')
    sources = [HERE/'boot_host.c', HERE/'net_disabled.c', portable/'i386.c']
    sources += [args.tiny386/name for name in core_names]
    sources += [portable/name for name in ('pci.c', 'vga.c', 'adlib.c')]
    exe = args.output/'boot_host.exe'
    command = ['cl', '/nologo', '/O2', '/std:c11', '/Zc:preprocessor', '/wd4244', '/wd4267',
               '/I'+str(HERE), '/I'+str(args.tiny386), '/FI'+str(HERE/'host_compat.h')]
    command += [str(path) for path in sources] + ['/Fe:'+str(exe)]
    build = subprocess.run(command, cwd=args.output, capture_output=True)
    (args.output/'build.log').write_bytes(build.stdout+build.stderr)
    if build.returncode: raise SystemExit('Host compilation failed; see '+str(args.output/'build.log'))
    result = subprocess.run([str(exe), str(args.bios), str(args.vgabios), str(image)],
                            cwd=args.output, capture_output=True, timeout=30)
    (args.output/'boot.stdout.txt').write_bytes(result.stdout)
    (args.output/'boot.stderr.txt').write_bytes(result.stderr)
    text = result.stdout.decode(errors='replace')
    markers = {name: name in text for name in ('TAB5 386 DOS OK', 'TAB5 486 DOS OK',
               'TAB5 PM32 RETURN OK', 'TAB5 PROTOTYPE DONE')}
    report = {'tiny386_commit':PIN, 'target':'Windows x64 host only; not ESP32-P4',
              'passed':result.returncode == 0 and all(markers.values()) and 'DOS FAIL' not in text,
              'markers':markers, 'guest_ram_bytes':8*1024*1024, 'vga_ram_bytes':1024*1024,
              'canvas_bytes':720*480*2, 'input_freedos_sha256':hashlib.sha256(original).hexdigest(),
              'probe_image_sha256':digest(image), 'bios_sha256':digest(args.bios),
              'vgabios_sha256':digest(args.vgabios),
              'program_sha256':{name:digest(args.output/name) for name in ('TEST386.COM','TEST486.EXE','TESTPM.COM')},
              'limits':['No target ELF or physical performance measurement',
                        'No DOS extender, DPMI, paging or full 486 conformance test',
                        'No mouse, sound, SDK or allocation-failure integration test']}
    (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    if not report['passed']: raise SystemExit(1)


if __name__ == '__main__': run()
