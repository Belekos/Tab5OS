#!/usr/bin/env python3
"""Reproduce the minimal Tab5OS FreeDOS floppy and its corresponding source pack.

Python 3 standard library only. Release inputs and extracted binaries are pinned
by SHA-256. Never replaces an existing different image without --force.

The FAT12 image construction is adapted from esp32pc/tools/fatmk.py:
Copyright (c) 2026 Robert Lucencic, MIT License (full text below).
Other code in this file: Copyright (c) 2026 Tab5OS contributors, MIT License.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import urllib.request
import zipfile

INPUTS = {
    'FD14-FloppyEdition.zip': ('https://download.freedos.org/1.4/FD14-FloppyEdition.zip',
        '45b1fa7c52dd996c3bfa5e352ffcd410781b952a6ad629f15a4c9ec4bbaefc5a'),
    'FD14-LiteUSB.zip': ('https://download.freedos.org/1.4/FD14-LiteUSB.zip',
        '857dcd2ebf9d3d094320154db5fb5b830acba6fb98f981a95a0ca7ab3350338b'),
}
EXPECTED = {
    'KERNEL.SYS': 'f34a7483c575fcf2709d9a7d0bc3db81c6211c279530f9e1bf78576b9233924d',
    'COMMAND.COM': '077808379e896476f7f69d62e6c8989d8fc23e8ef58d1c8492db1ac106784107',
    'KERNEL.ZIP': '38ce3c63e399c8f18ab6230d8988a5d1a1aa9be4e109d15c6f4842b5e8fe61e6',
    'FREECOM.ZIP': '2529cf15c2ee7d7030ed99a6a88df2cf5eef87b9fe10f1c0fb643c38ea6aaa8e',
    'kernel-source.zip': 'fda372721899e6a9cabbfa6beed259be19dafd0fe16d90b8f4da1a9397fde574',
    'freecom-source.zip': 'beef029a2268cac4dd5b729a649ea4e23f30c9cfee1788c4bbd64b4d4ddb093b',
    'KEYB.ZIP': '9fd172421cfd6f30bf45cd9e46e45c78e40439209104c4e81498e327904e759e',
    'KEYB_LAY.ZIP': 'c20e750bd97eb85a1210500ab7f756f38a2070359eb53791a6a5979070f30bf6',
    'keyb-source.zip': '649d94d496c61565147774d222302199d603e44d3412dd0e0e9f5b68d73ce0de',
}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def checked(data, expected, name):
    actual = sha(data)
    if actual != expected:
        raise ValueError(f'{name}: SHA-256 mismatch: {actual}, expected {expected}')
    return data

def fetch(cache, name, offline):
    url, digest = INPUTS[name]
    path = cache / name
    if not path.exists():
        if offline:
            raise FileNotFoundError(f'Missing cached input: {path}')
        cache.mkdir(parents=True, exist_ok=True)
        partial = path.with_suffix(path.suffix + '.part')
        print(f'Downloading {url}')
        with urllib.request.urlopen(url, timeout=45) as source, partial.open('wb') as target:
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                target.write(chunk)
        checked(partial.read_bytes(), digest, name)
        partial.replace(path)
    return checked(path.read_bytes(), digest, name)

class FAT:
    """Bounded read-only FAT12/FAT16 reader for the two pinned release images."""
    def __init__(self, raw):
        self.raw = raw
        if raw[510:512] != b'\x55\xaa':
            raise ValueError('Missing FAT boot signature')
        self.bps, self.spc, self.res, self.nfat, self.nroot, total, _, self.spf = struct.unpack_from('<HBHBHHBH', raw, 11)
        if self.bps != 512 or self.spc not in (1, 2, 4, 8, 16, 32, 64):
            raise ValueError('Unsupported FAT geometry')
        self.root = (self.res + self.nfat*self.spf)*self.bps
        self.data = self.root + ((self.nroot*32+self.bps-1)//self.bps)*self.bps
        self.cluster_size = self.bps*self.spc
        total = total or struct.unpack_from('<I', raw, 32)[0]
        self.clusters = (total-self.data//self.bps)//self.spc
        self.fat16 = self.clusters >= 4085
        if self.clusters >= 65525 or total*self.bps > len(raw):
            raise ValueError('Unsupported or truncated FAT volume')

    def chain(self, cluster):
        chunks, visited = [], set()
        while 2 <= cluster < (0xfff8 if self.fat16 else 0xff8):
            if cluster in visited or cluster >= self.clusters+2:
                raise ValueError('Invalid FAT chain')
            visited.add(cluster)
            pos = self.data+(cluster-2)*self.cluster_size
            chunks.append(self.raw[pos:pos+self.cluster_size])
            if self.fat16:
                cluster = struct.unpack_from('<H', self.raw, self.res*self.bps+cluster*2)[0]
            else:
                value = struct.unpack_from('<H', self.raw, self.res*self.bps+cluster*3//2)[0]
                cluster = value >> 4 if cluster & 1 else value & 0xfff
        return b''.join(chunks)

    def files(self, directory=None, prefix='', depth=0):
        if depth > 16:
            raise ValueError('FAT directory nesting exceeds limit')
        if directory is None:
            directory = self.raw[self.root:self.root+self.nroot*32]
        for offset in range(0, len(directory), 32):
            entry = directory[offset:offset+32]
            if len(entry) < 32 or entry[0] == 0:
                break
            if entry[0] == 0xe5 or entry[11] & 8:
                continue
            name = entry[:8].decode('ascii').rstrip()
            ext = entry[8:11].decode('ascii').rstrip()
            if ext:
                name += '.' + ext
            if name in ('.', '..'):
                continue
            cluster, size = struct.unpack_from('<HI', entry, 26)
            content = self.chain(cluster) if cluster else b''
            path = prefix+name
            if entry[11] & 16:
                yield from self.files(content, path+'/', depth+1)
            else:
                if size > len(content):
                    raise ValueError('Truncated FAT file: '+path)
                yield path, content[:size]

def make_floppy(boot, files):
    if len(boot) != 1474560:
        raise ValueError('Expected a 1.44 MB source floppy')
    bps, spc, reserved, fats, roots, total, media, spf = struct.unpack_from('<HBHBHHBH', boot, 11)
    if (bps, spc, reserved, fats, roots, total, spf) != (512, 1, 1, 2, 224, 2880, 9):
        raise ValueError('Unexpected floppy geometry')
    image = bytearray(len(boot))
    image[:512] = boot[:512]
    root = (reserved+fats*spf)*bps
    data = root+roots*32
    fat = bytearray(spf*bps)
    fat[:3] = bytes((media, 255, 255))
    def put_fat(cluster, value):
        offset = cluster*3//2
        word = struct.unpack_from('<H', fat, offset)[0]
        word = (word & 0x000f) | value << 4 if cluster & 1 else (word & 0xf000) | value
        struct.pack_into('<H', fat, offset, word)
    cluster = 2
    for index, (name, content) in enumerate(files.items()):
        if index >= roots:
            raise ValueError('Too many root entries')
        base, _, ext = name.partition('.')
        if not 1 <= len(base) <= 8 or len(ext) > 3:
            raise ValueError('Expected a DOS 8.3 name')
        count = (len(content)+bps-1)//bps
        first = cluster if count else 0
        if data+(cluster-2+count)*bps > len(image):
            raise ValueError('Boot floppy is full')
        for index_in_file in range(count):
            put_fat(cluster, cluster+1 if index_in_file+1<count else 0xfff)
            offset = data+(cluster-2)*bps
            image[offset:offset+bps] = content[index_in_file*bps:(index_in_file+1)*bps].ljust(bps,b'\0')
            cluster += 1
        entry = bytearray(32)
        entry[:11] = (base.ljust(8)+ext.ljust(3)).encode('ascii')
        entry[11] = 0x20
        struct.pack_into('<H', entry, 24, ((2025-1980)<<9) | (1<<5) | 1)
        struct.pack_into('<HI', entry, 26, first, len(content))
        image[root+index*32:root+(index+1)*32] = entry
    for index in range(fats):
        offset = reserved*bps+index*spf*bps
        image[offset:offset+len(fat)] = fat
    result = bytes(image)
    if dict(FAT(result).files()) != files:
        raise ValueError('Generated image validation failed')
    return result

def write_file(path, content, force=True):
    if path.exists():
        if path.read_bytes() == content:
            return
        if not force:
            raise FileExistsError(f'Refusing to replace changed image: {path}; use --force only for a disposable boot image')
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name+'.tmp')
    temporary.write_bytes(content)
    temporary.replace(path)

def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--cache-dir', type=Path, default=Path(__file__).parent/'downloads')
    parser.add_argument('--output', type=Path, default=Path('dos-output/freedos.img'))
    parser.add_argument('--licenses-dir', type=Path, default=Path('dos-output/freedos-sources'))
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--keyboard-tools', action='store_true', help='include optional KEYB and international layouts, leaving the default US layout unchanged')
    args = parser.parse_args()
    floppy_release = zipfile.ZipFile(io.BytesIO(fetch(args.cache_dir, 'FD14-FloppyEdition.zip', args.offline)))
    boot = floppy_release.read('144m/x86BOOT.img')
    boot_files = dict(FAT(boot).files())
    kernel = checked(boot_files['KERNEL.SYS'], EXPECTED['KERNEL.SYS'], 'KERNEL.SYS')
    command = checked(boot_files['FREEDOS/BIN/COMMAND.COM'], EXPECTED['COMMAND.COM'], 'COMMAND.COM')
    usb_release = zipfile.ZipFile(io.BytesIO(fetch(args.cache_dir, 'FD14-LiteUSB.zip', args.offline)))
    usb = usb_release.read('FD14LITE.img')
    partition_lba = struct.unpack_from('<I', usb, 454)[0]
    usb_files = dict(FAT(usb[partition_lba*512:]).files())
    packages = {}
    for name in ('KERNEL', 'FREECOM'):
        data = checked(usb_files[f'PACKAGES/BASE/{name}.ZIP'], EXPECTED[name+'.ZIP'], name+'.ZIP')
        packages[name] = zipfile.ZipFile(io.BytesIO(data))
    if packages['KERNEL'].read('BIN/KERNL86.SYS') != kernel:
        raise ValueError('8086 kernel does not match the source package binary')
    if packages['FREECOM'].read('BIN/COMMAND.COM') != command:
        raise ValueError('Shell does not match the source package binary')
    config = 'LASTDRIVE=Z\r\nFILES=20\r\nBUFFERS=10\r\nSHELL=A:\\COMMAND.COM A:\\ /E:512 /P=A:\\AUTOEXEC.BAT\r\n'
    autoexec = '@ECHO OFF\r\nSET PATH=A:\\\r\nSET COMSPEC=A:\\COMMAND.COM\r\nPROMPT $P$G\r\nECHO FreeDOS 1.4 for Tab5OS\r\nECHO TAB5OS DOS READY\r\n'
    files = {'KERNEL.SYS':kernel, 'COMMAND.COM':command, 'CONFIG.SYS':config.encode('ascii'), 'AUTOEXEC.BAT':autoexec.encode('ascii')}
    keyboard_packages = {}
    if args.keyboard_tools:
        for name in ('KEYB', 'KEYB_LAY'):
            data = checked(usb_files[f'PACKAGES/BASE/{name}.ZIP'], EXPECTED[name+'.ZIP'], name+'.ZIP')
            keyboard_packages[name] = (data, zipfile.ZipFile(io.BytesIO(data)))
        files['KEYB.EXE'] = keyboard_packages['KEYB'][1].read('BIN/KEYB.EXE')
        files['KEYBOARD.SYS'] = keyboard_packages['KEYB_LAY'][1].read('BIN/KEYBOARD.SYS')
    image = make_floppy(boot, files)
    if args.output.exists() and args.output.read_bytes() != image and not args.force:
        raise FileExistsError(f'Refusing to replace changed image: {args.output}; choose a new output or use --force')
    sources = {}
    for package, stem in (('KERNEL','kernel'),('FREECOM','freecom')):
        source_name = stem+'-source.zip'
        source = checked(packages[package].read(f'SOURCE/{package}/SOURCES.ZIP'), EXPECTED[source_name], source_name)
        sources[stem] = zipfile.ZipFile(io.BytesIO(source))
        write_file(args.licenses_dir/source_name, source)
        write_file(args.licenses_dir/(stem+'-package.lsm'), packages[package].read(f'APPINFO/{package}.LSM'))
    write_file(args.licenses_dir/'LICENSE-kernel.txt', sources['kernel'].read('COPYING'))
    write_file(args.licenses_dir/'LICENSE-freecom.txt', sources['freecom'].read('license'))
    write_file(args.licenses_dir/'LICENSE-freecom-suppl.txt', sources['freecom'].read('suppl/src/copying'))
    write_file(args.licenses_dir/'LICENSE-freecom-suppl-library.txt', sources['freecom'].read('suppl/src/copying.lb'))
    if args.keyboard_tools:
        keyb = keyboard_packages['KEYB'][1]
        layouts = keyboard_packages['KEYB_LAY'][1]
        keyb_source = checked(keyb.read('SOURCE/KEYB/SOURCES.ZIP'), EXPECTED['keyb-source.zip'], 'keyb-source.zip')
        write_file(args.licenses_dir/'keyb-source.zip', keyb_source)
        # This package stores the complete original .KEY sources directly, rather
        # than in a nested source archive. Keep the package byte for byte.
        write_file(args.licenses_dir/'keyb-layouts-3.1a.zip', keyboard_packages['KEYB_LAY'][0])
        write_file(args.licenses_dir/'LICENSE-keyb.txt', keyb.read('DOC/KEYB/COPYING.TXT'))
        write_file(args.licenses_dir/'LICENSE-keyb-layouts.txt', layouts.read('DOC/KEYB/LAYOUTS/COPYING.TXT'))
        write_file(args.licenses_dir/'keyb-package.lsm', keyb.read('APPINFO/KEYB.LSM'))
        write_file(args.licenses_dir/'keyb-layouts-package.lsm', layouts.read('APPINFO/KEYB_LAY.LSM'))
        write_file(args.licenses_dir/'KEYB-HELP.txt', keyb.read('HELP/KEYB'))
    readme = '''# FreeDOS boot image for Tab5OS

This is an unofficial, minimal image derived from the official FreeDOS 1.4
release. FreeDOS is a free DOS-compatible operating system; it is not Microsoft
MS-DOS. Project: https://www.freedos.org/

Included guest software:

- FreeDOS kernel 2043 (2021-05-14), the 8086-compatible KERNL86.SYS build,
  renamed KERNEL.SYS without modifying its bytes. Upstream: https://github.com/FDOS/kernel
- FreeCOM package 0.86a (2025-02-21), COMMAND.COM. Its embedded version text is
  "FreeCom version 0.86 - WATCOMC - XMS_Swap [Dec 30 2024 22:10:51]". Upstream: https://github.com/FDOS/freecom
- The FreeDOS FAT12 boot sector from 144m/x86BOOT.img, plus Tab5OS CONFIG.SYS
  and AUTOEXEC.BAT. No Microsoft binaries, games, installer, or memory drivers.

The kernel and FreeCOM packages identify their licence as GNU GPL version 2.
Their original licence texts and notices are preserved here and in the source
archives. FreeCOM's SUPPL library licence texts are also included. The source
archives are copied byte for byte from the same official release's packages;
the shipped kernel and shell are checked against those packages' binaries.

Keep this entire source-and-licence directory with any redistributed boot image.
kernel-source.zip and freecom-source.zip contain the corresponding source,
copyright notices and build scripts. For rebuilding, see docs/build.txt and
README.md in the kernel source, and docs/compile.txt plus build scripts in the
FreeCOM source. Required toolchains are described in those upstream documents.

Changes from the official release: the boot sector, kernel, shell and selected
keyboard tools are retained; the disk is reformatted as a 1.44 MB FAT12 floppy with fixed dates;
CONFIG.SYS and AUTOEXEC.BAT start an English command prompt without the installer.
Guest application compatibility is limited by the selected emulator CPU model.

Reproduce with Python 3 (standard library only):

    python prepare_freedos.py --cache-dir downloads --output freedos.img --licenses-dir freedos-sources

Add --offline to use previously downloaded, verified archives. Different existing
output images are preserved unless --force is explicitly supplied. Input release
URLs, SHA-256 digests, exact payload hashes and the generated image digest are
recorded in provenance.json. Official release checksums:
https://www.freedos.org/download/verify.txt
'''
    if args.keyboard_tools:
        readme = readme.replace('--licenses-dir freedos-sources\n', '--licenses-dir freedos-sources --keyboard-tools\n')
        readme += '''
Optional keyboard tools are included in this build: KEYB 2.11 by Aitor Santamaria
Merino and keyboard layouts 3.1a by Henrique Peron, under GNU GPL version 2 as
their package metadata states (the KEYB README also permits later versions).
Matching sources, licence texts and help are included in this directory.
No keyboard driver is automatically loaded. At the DOS prompt, the command
`KEYB FR,437,A:\\KEYBOARD.SYS` selects the standard French AZERTY layout for
hardware scancodes. `KEYB /U` unloads the driver and restores the BIOS layout.
The emulator's own text injection and touch keyboard must use the selected
guest layout; an emulator frontend may therefore keep its documented US layout.
Build this optional variant with `--keyboard-tools`.
'''
    write_file(args.licenses_dir/'README.md', readme.encode('utf-8'))
    provenance = {
        'description':'Unofficial minimal FreeDOS 1.4 boot floppy for Tab5OS',
        'inputs':{name:{'url':url,'sha256':digest} for name,(url,digest) in INPUTS.items()},
        'source_boot_image':{'member':'144m/x86BOOT.img','sha256':sha(boot)},
        'versions':{'kernel':'2043, KERNL86.SYS (8086)','freecom_package':'0.86a'},
        'optional_keyboard_tools':args.keyboard_tools,
        'image':{'size':len(image),'sha256':sha(image)},
        'files':{name:{'size':len(content),'sha256':sha(content)} for name,content in files.items()},
        'extracted_release_objects_sha256':EXPECTED,
        'builder_origin':'https://github.com/rlucencic/esp32pc/blob/ebfd36fa62d2a93e6eabc7a6c9853b839826127f/tools/fatmk.py',
    }
    if args.keyboard_tools:
        provenance['versions'].update(keyb='2.11', keyb_layouts='3.1a')
    write_file(args.licenses_dir/'provenance.json',(json.dumps(provenance,indent=2,sort_keys=True)+'\n').encode('utf-8'))
    write_file(args.output,image,args.force)
    print(f'{args.output}: {len(image)} bytes; SHA-256 {sha(image)}')
    print(f'Sources and licences: {args.licenses_dir}')

if __name__ == '__main__':
    main()
