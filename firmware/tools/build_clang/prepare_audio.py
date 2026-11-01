"""Make an isolated ESP8266Audio copy with its PDM rate conversion explicit."""
import hashlib
import json
from pathlib import Path
import shutil
from prepare_m5gfx import files_manifest


def prepare_audio(source, work):
    source=Path(source).resolve()
    work=Path(work).resolve()
    destination=work/'dependencies'/'ESP8266Audio'
    relative='src/AudioOutputPDM.cpp'
    original=(source/relative).read_bytes()
    expected_original='20e7c6ac2f97f7cf228da95ad445a5fa3270bfe4cf9a59ea499b8ac2a5ba3641'
    if hashlib.sha256(original.replace(b'\r\n',b'\n')).hexdigest()!=expected_original:
        raise RuntimeError('AudioOutputPDM.cpp does not match the verified ESP8266Audio source SHA-256.')
    before=b'I2S_PDM_TX_CLK_DEFAULT_CONFIG(AdjustI2SRate(hertz))'
    after=b'I2S_PDM_TX_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(AdjustI2SRate(hertz)))'
    if original.count(before)!=1:
        raise RuntimeError('Unexpected AudioOutputPDM.cpp source; refusing an unchecked patch.')
    patched=original.replace(before,after)
    expected=files_manifest(source)
    original_hash=expected[relative]
    expected[relative]=hashlib.sha256(patched).hexdigest()
    if destination.exists():
        if files_manifest(destination)!=expected:
            raise RuntimeError('Existing isolated ESP8266Audio copy differs. Use a new --work-dir.')
    else:
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copytree(source,destination)
        (destination/relative).write_bytes(patched)
    report={'library':'ESP8266Audio','tag':'2.4.2','metadataVersion':'2.4.1','source':str(source),'isolatedCopy':str(destination),
            'file':relative,'originalSha256':original_hash,'patchedSha256':expected[relative],
            'changes':[{'before':before.decode(),'after':after.decode()}],
            'originalInstallationModified':False,'filesChecked':len(expected)}
    (work/'audio-clang-patch.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return destination
