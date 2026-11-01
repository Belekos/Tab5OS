"""Prepare an isolated M5GFX 0.2.30 copy with bounded JPEG decoder fixes."""
from pathlib import Path
import argparse, hashlib, json, shutil

DECODER = 'src/lgfx/utility/lgfx_tjpgd.c'
UPSTREAM_SHA256 = '3b2dbd5b3aa0135b3af9557fab936f3acc0873cc638d35111510487ea393bd4a'
REPLACEMENTS = [
    (b'v4 = (v4 << 1) - v5;', b'v4 = src[1] - src[7];'),
    (b'v6 = (v6 << 1) - v7;', b'v6 = src[5] - src[3];'),
    (b'v7 += v5;\n\t\tv5 = (v5 << 1) - v7;', b't10 = v5 - v7;\n\t\tv7 += v5;\n\t\tv5 = t10;'),
    (b't11 = (v1 << 1) - v3;', b't11 = src[2] - src[6];'),
    (b'memcpy(d += rx * 3, s_ += mx * 3, rx * 3);', b'memmove(d += rx * 3, s_ += mx * 3, rx * 3);'),
]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def files_manifest(root):
    return {p.relative_to(root).as_posix():digest(p.read_bytes()) for p in sorted(root.rglob('*')) if p.is_file()}


def patch_decoder(original):
    if digest(original) != UPSTREAM_SHA256:
        raise RuntimeError('Unexpected M5GFX 0.2.30 JPEG decoder; refusing unchecked patch')
    patched = original
    for before, after in REPLACEMENTS:
        if patched.count(before) != 1:
            raise RuntimeError('Unexpected JPEG expression: '+before.decode())
        patched = patched.replace(before, after)
    return patched


def prepare_m5gfx_jpeg(source, work):
    source, work = Path(source).resolve(), Path(work).resolve()
    destination = work/'dependencies/M5GFX'
    original = (source/DECODER).read_bytes()
    patched = patch_decoder(original)
    expected = files_manifest(source)
    expected[DECODER] = digest(patched)
    if destination.exists():
        if files_manifest(destination) != expected:
            raise RuntimeError('Existing isolated M5GFX copy differs. Use a new work directory.')
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, destination)
        (destination/DECODER).write_bytes(patched)
    report = {'library':'M5GFX', 'version':'0.2.30', 'source':str(source), 'isolatedCopy':str(destination), 'file':DECODER, 'originalSha256':digest(original), 'patchedSha256':digest(patched), 'originalInstallationModified':False, 'filesChecked':len(expected), 'changes':[{'before':a.decode(), 'after':b.decode()} for a,b in REPLACEMENTS]}
    (work/'m5gfx-jpeg-patch.json').write_text(json.dumps(report, indent=2)+'\n')
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    args = parser.parse_args()
    print(prepare_m5gfx_jpeg(args.source, args.work))
