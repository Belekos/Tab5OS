"""Small synthetic archives; no Wikipedia download is needed for the tests."""
import lzma
from pathlib import Path
import struct
import sys

destination = Path(sys.argv[1]); destination.mkdir(parents=True, exist_ok=True)
html = b'<p>' + b'Long article text and repeated words.\n' * 90000 + b'<a href="Next">Next</a></p>'
html += b'<table><tr><th rowspan="2">Region</th><th colspan="2">Population</th></tr><tr><td><a href="Next">Next</a></td><td>Second</td></tr></table><p>After table</p>'
raw = struct.pack('<III', 12, 12 + len(html), 12 + len(html) + 4) + html + b'Next'
xz = lzma.compress(raw, format=lzma.FORMAT_XZ, filters=[{'id': lzma.FILTER_LZMA2, 'dict_size': 64 * 1024 * 1024}])
(destination / 'cluster.raw').write_bytes(raw); (destination / 'cluster.xz').write_bytes(xz)
# A standards-compliant Zstandard single-segment frame with raw blocks.
zstd = b'\x28\xb5\x2f\xfd\xa0' + struct.pack('<I', len(raw))
for at in range(0, len(raw), 65536):
    block = raw[at:at + 65536]
    zstd += ((len(block) << 3) | int(at + len(block) == len(raw))).to_bytes(3, 'little') + block

for name, compression, payload in [('plain', 1, raw), ('xz', 4, xz), ('zstd', 5, zstd)]:
    archive = bytearray(80) + b'text/html\0text/plain\0\0'
    paths = len(archive); archive += b'\0' * 16
    titles = len(archive); archive += struct.pack('<II', 0, 1)
    clusters = len(archive); archive += b'\0' * 8
    for index, path in enumerate([b'Article', b'Next']):
        struct.pack_into('<Q', archive, paths + index * 8, len(archive))
        archive += struct.pack('<HBBIII', index, 0, ord('C'), 0, 0, index) + path + b'\0' + path + b'\0'
    struct.pack_into('<Q', archive, clusters, len(archive))
    archive += bytes([compression]) + payload
    struct.pack_into('<IHH', archive, 0, 0x044d495a, 6, 1)
    struct.pack_into('<IIQQQQ', archive, 24, 2, 1, paths, titles, clusters, 80)
    (destination / (name + '.zim')).write_bytes(archive)
print('Fixtures written:', len(raw), 'decoded bytes; XZ dictionary 64 MiB')
