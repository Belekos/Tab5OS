"""Make an isolated LVGL copy with an allocation-safe Arabic text shaper.

LVGL 9.6.0's shaper allocates two character buffers. On allocation failure,
preserve the original UTF-8 label instead of dereferencing a null buffer.
The original Arduino library installation is never modified.
"""
import hashlib
import json
from pathlib import Path
import shutil
from prepare_m5gfx import files_manifest


def patch_shaper(original):
    expected_original = '111d15c7c6ac956892980432b30a7c7aa46382d278855442cdc2a5064d82d689'
    if hashlib.sha256(original.replace(b'\r\n', b'\n')).hexdigest() != expected_original:
        raise RuntimeError('lv_text_ap.c does not match the verified LVGL 9.6.0 source SHA-256.')
    newline = b'\r\n' if b'\r\n' in original else b'\n'
    before = newline.join([
        b'    ch_enc = (uint32_t *)lv_malloc(sizeof(uint32_t) * (txt_length + 1));',
        b'    ch_fin = (uint32_t *)lv_malloc(sizeof(uint32_t) * (txt_length + 1));',
    ])
    after = before + newline + newline + newline.join([
        b'    /* Tab5OS: keep the original label if shaping has no working memory. */',
        b'    if(ch_enc == NULL || ch_fin == NULL) {',
        b'        lv_free(ch_enc);',
        b'        lv_free(ch_fin);',
        b'        if(txt_out != txt) {',
        b'            const char * source = txt;',
        b'            char * destination = txt_out;',
        b'            while((*destination++ = *source++) != 0) {}',
        b'        }',
        b'        return;',
        b'    }',
    ])
    if original.count(before) != 1:
        raise RuntimeError('Unexpected lv_text_ap.c source; refusing an unchecked patch.')
    return original.replace(before, after), before, after


def prepare_lvgl(source, work):
    source = Path(source).resolve()
    work = Path(work).resolve()
    destination = work / 'dependencies' / 'lvgl'
    relative = 'src/misc/lv_text_ap.c'
    original = (source / relative).read_bytes()
    patched, before, after = patch_shaper(original)
    expected = files_manifest(source)
    original_hash = expected[relative]
    expected[relative] = hashlib.sha256(patched).hexdigest()
    if destination.exists():
        if files_manifest(destination) != expected:
            raise RuntimeError('Existing isolated LVGL copy differs. Use a new --work-dir.')
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source, destination)
        (destination / relative).write_bytes(patched)
    report = {
        'library': 'LVGL', 'version': '9.6.0', 'source': str(source),
        'isolatedCopy': str(destination), 'file': relative,
        'originalSha256': original_hash, 'patchedSha256': expected[relative],
        'changes': [{'before': before.decode(), 'after': after.decode()}],
        'originalInstallationModified': False, 'filesChecked': len(expected),
    }
    (work / 'lvgl-arabic-patch.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return destination


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--work', required=True, type=Path)
    options = parser.parse_args()
    print(prepare_lvgl(options.source, options.work))
