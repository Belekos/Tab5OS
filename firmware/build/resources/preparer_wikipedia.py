#!/usr/bin/env python3
"""Copy a complete Wikipedia ZIM to a Tab5OS SD card as 64 MiB parts.

Example (Windows):
  py preparer_wikipedia.py C:\\Downloads\\wikipedia_fr_all_nopic_2026-05.zim E:\\

Use the exact official Kiwix filename. The destination is the SD card root,
not an individual file. A temporary folder on a PC is also accepted: copy its
Wikipedia folder to the SD afterwards. No third-party Python package is needed.

An interrupted copy resumes by running the same command. The source SHA-256,
every completed part and the final assembled SHA-256 are checked. Copying and
hashing use 1 MiB buffers. FAT32 is supported; the complete archive never needs
to fit in one file. A partial PC copy must be resumed with this tool, not with
the tablet's network downloader. Existing unrelated files are not removed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys

PART_SIZE = 64 * 1024 * 1024
BLOCK_SIZE = 1024 * 1024
FORMAT = 'tab5-zim-parts-v1'
NAME = re.compile(r'wikipedia_([a-z0-9-]{2,20})_all_(nopic|maxi)_([0-9]{4}-[0-9]{2})\.zim\Z')


def atomic_bytes(path, data):
    temporary = path.with_name(path.name + '.writing')
    with temporary.open('wb') as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def save(path, state):
    atomic_bytes(path, (json.dumps(state, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))


def hash_file(path, assembled=None):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(BLOCK_SIZE), b''):
            digest.update(block)
            if assembled is not None:
                assembled.update(block)
    return digest.hexdigest()


def prepare(source, destination):
    source = source.resolve(strict=True)
    match = NAME.fullmatch(source.name)
    if not match or not 1 <= int(match[3][5:]) <= 12:
        raise ValueError('Keep the official filename: wikipedia_LANGUAGE_all_nopic_DATE.zim or ..._maxi_DATE.zim.')
    if not source.is_file():
        raise ValueError('The source must be a ZIM file.')
    original = source.stat()
    if original.st_size < 96 or original.st_size > PART_SIZE * 1000000:
        raise ValueError('Unsupported archive size.')
    with source.open('rb') as stream:
        header = stream.read(80)
    if header[:4] != b'ZIM\x04' or int.from_bytes(header[4:6], 'little') not in (5, 6):
        raise ValueError('This is not a supported ZIM version 5 or 6 archive.')
    print('Checking source SHA-256...', flush=True)
    source_digest = hash_file(source)
    if (source.stat().st_size, source.stat().st_mtime_ns) != (original.st_size, original.st_mtime_ns):
        raise ValueError('The source changed while it was being checked.')
    folder = destination.resolve() / 'Wikipedia' / match[1] / 'archives' / (source.name + 'parts')
    manifest = folder / 'manifest.json'
    if manifest.exists():
        state = json.loads(manifest.read_text(encoding='utf-8'))
        if (state.get('format') != FORMAT or state.get('transfer') != 'manual' or
                state.get('filename') != source.name or state.get('size') != original.st_size or
                state.get('sha256') != source_digest or state.get('part_size') != PART_SIZE):
            raise ValueError('This destination belongs to a different archive or network download. Use a separate destination.')
    else:
        folder.mkdir(parents=True, exist_ok=True)
        if any(folder.iterdir()):
            raise ValueError('The destination archive folder already contains unrecognized files.')
        state = {'format': FORMAT, 'filename': source.name, 'language': match[1],
                 'variant': match[2], 'date': match[3], 'title': 'Wikipedia',
                 'part_size': PART_SIZE, 'size': original.st_size, 'done': 0,
                 'complete': False, 'transfer': 'manual', 'sha256': source_digest,
                 'etag': '', 'last_modified': '',
                 'source': 'https://download.kiwix.org/zim/wikipedia/' + source.name}
        save(manifest, state)
    state['complete'] = False
    state['done'] = 0
    save(manifest, state)
    assembled = hashlib.sha256()
    count = (original.st_size + PART_SIZE - 1) // PART_SIZE
    with source.open('rb') as incoming:
        for index in range(count):
            length = min(PART_SIZE, original.st_size - index * PART_SIZE)
            part = folder / ('part-%06d.bin' % index)
            digest_path = folder / ('part-%06d.sha256' % index)
            temporary = part.with_name(part.name + '.writing')
            existing = False
            if part.is_file() and part.stat().st_size == length and digest_path.is_file():
                expected = digest_path.read_text(encoding='ascii').strip()
                candidate = assembled.copy()
                existing = len(expected) == 64 and hash_file(part, candidate) == expected
                if existing:
                    assembled = candidate
            if not existing:
                if temporary.exists():
                    temporary.unlink()
                if part.exists():
                    part.unlink()
                if shutil.disk_usage(folder).free < length + 65536:
                    raise OSError('Not enough destination space. Free space and run the same command to resume.')
                incoming.seek(index * PART_SIZE)
                digest = hashlib.sha256()
                with temporary.open('wb') as outgoing:
                    remaining = length
                    while remaining:
                        block = incoming.read(min(BLOCK_SIZE, remaining))
                        if not block:
                            raise OSError('Unexpected end of source archive.')
                        if outgoing.write(block) != len(block):
                            raise OSError('Short write on destination.')
                        digest.update(block)
                        assembled.update(block)
                        remaining -= len(block)
                    outgoing.flush()
                    os.fsync(outgoing.fileno())
                os.replace(temporary, part)
                atomic_bytes(digest_path, (digest.hexdigest() + '\n').encode('ascii'))
            if (source.stat().st_size, source.stat().st_mtime_ns) != (original.st_size, original.st_mtime_ns):
                raise ValueError('The source changed during the copy; the archive has not been committed.')
            final = index + 1 == count
            if final and assembled.hexdigest() != source_digest:
                raise ValueError('The assembled archive checksum does not match the source. The archive has not been committed.')
            state['done'] = min((index + 1) * PART_SIZE, original.st_size)
            state['complete'] = final
            save(manifest, state)
            percent = 100.0 if final else min(99.9, 100 * state['done'] / state['size'])
            print('%d / %d parts · %.1f%%' % (index + 1, count, percent), flush=True)
    print('Ready: ' + str(folder), flush=True)
    return folder


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('source', type=Path, help='Official Wikipedia all_nopic or all_maxi ZIM file')
    parser.add_argument('destination', type=Path, help='SD card root or an empty staging folder on the PC')
    args = parser.parse_args()
    try:
        prepare(args.source, args.destination)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print('Stopped: ' + str(error), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print('\nPaused. Run the same command to resume.', file=sys.stderr)
        return 130
    return 0


if __name__ == '__main__':
    sys.exit(main())
