"""Make a checked, local M5GFX 0.2.30 copy compatible with standard C++."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'build_linux'))
from prepare_m5gfx_jpeg import patch_decoder, DECODER


def files_manifest(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}


def prepare_m5gfx(source, work):
    source=Path(source).resolve()
    work=Path(work).resolve()
    destination=work/'dependencies'/'M5GFX'
    relative='src/lgfx/v1/platforms/esp32/common.inl'
    original=(source/relative).read_bytes()
    expected_original='95634d12f9c4115d09226779d7e0539c039de83a8ed6a6dd17ab1398d17c02f2'
    if hashlib.sha256(original.replace(b'\r\n',b'\n')).hexdigest()!=expected_original:
        raise RuntimeError('M5GFX common.inl does not match the verified 0.2.30 source SHA-256.')
    replacements=[
        (b'static constexpr volatile uint32_t* gpio_en_regs[] =',
         b'static volatile uint32_t* const gpio_en_regs[] ='),
        (b': _pin_num { static_cast<gpio_num_t>(pin_num) }',
         b': _pin_num { static_cast<decltype(_pin_num)>(pin_num) }'),
    ]
    patched=original
    for before,after in replacements:
        if patched.count(before)!=1:
            raise RuntimeError('Unexpected M5GFX 0.2.30 source; refusing an unchecked patch: '+before.decode())
        patched=patched.replace(before,after)
    jpeg_original=(source/DECODER).read_bytes()
    jpeg_patched=patch_decoder(jpeg_original)
    expected=files_manifest(source)
    original_hash=expected[relative]
    expected[relative]=hashlib.sha256(patched).hexdigest()
    expected[DECODER]=hashlib.sha256(jpeg_patched).hexdigest()
    if destination.exists():
        if files_manifest(destination)!=expected:
            raise RuntimeError('Existing isolated M5GFX copy differs. Use a new --work-dir.')
    else:
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copytree(source,destination)
        (destination/relative).write_bytes(patched)
        (destination/DECODER).write_bytes(jpeg_patched)
    report={'library':'M5GFX','version':'0.2.30','source':str(source),'isolatedCopy':str(destination),
            'file':relative,'originalSha256':original_hash,'patchedSha256':expected[relative],
            'changes':[{'before':a.decode(),'after':b.decode()} for a,b in replacements],
            'originalInstallationModified':False,'filesChecked':len(expected),
            'jpegDecoderOriginalSha256':hashlib.sha256(jpeg_original).hexdigest(),
            'jpegDecoderPatchedSha256':hashlib.sha256(jpeg_patched).hexdigest()}
    (work/'m5gfx-clang-patch.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return destination
