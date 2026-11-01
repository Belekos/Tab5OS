"""Check the actual distributed resources and firmware recovery payload."""
import hashlib
import json
from pathlib import Path
import re
import zlib

root = Path(__file__).resolve().parents[1]
from release_config import VERSION, SKETCH, TAG
sketch = root / SKETCH
sd = sketch / 'sdcard'
manifest = json.loads((sd / f'Factory/Tab5OS-{VERSION}/manifest.json').read_text(encoding='utf8'))
assert manifest['version'] == VERSION
factory = sd / f'Factory/Tab5OS-{VERSION}'
assert {path.relative_to(factory).as_posix() for path in factory.rglob('*') if path.is_file()} == set(manifest['files']) | {'manifest.json'}
# The release ships English names; old user files are never moved here.
from rebuild_factory import RESOURCE_RENAMES
assert len(RESOURCE_RENAMES) == 11
assert not (set(RESOURCE_RENAMES) & set(manifest['files']))
for previous, current in RESOURCE_RENAMES.items():
    assert not (sd / previous).exists(), previous
    assert current in manifest['files'] and (sd / current).is_file(), current
assert {'Images/README.txt', 'Picture/README.txt', 'Applications/Pentest.app.elf', 'Applications/Pentest.app.elf.launch.json'} <= manifest['files'].keys()
assert (sketch / 'Tab5ImageEditor.cpp').is_file() and (sketch / 'Tab5ImageEditor.h').is_file()
assert json.loads((sd / 'Applications/Pentest.app.elf.launch.json').read_text(encoding='utf8')) == {'mode': 'none'}
for name, digest in manifest['files'].items():
    live = (sd / name).read_bytes()
    archived = (sd / f'Factory/Tab5OS-{VERSION}' / name).read_bytes()
    assert live == archived, name
    assert hashlib.sha256(live).hexdigest() == digest, name
assert not any((sketch / name).exists() for name in (
    'Tab5Mail.cpp', 'Tab5Browser.cpp', 'Tab5HtmlView.cpp', 'src/ReadyMail', 'src/litehtml'))
for path in (sd / 'MicroPython').rglob('*.py'):
    compile(path.read_text(encoding='utf8'), str(path), 'exec')
header = (sketch / 'Tab5SystemResourcesData.h').read_text(encoding='utf8')
blobs = {}
for number, literals in re.findall(r'static const uint8_t TAB5_RESOURCE_(\d+)\[\] = \{\n(.*?)\n\};', header, re.S):
    blobs[number] = bytes(int(n,16) for n in re.findall(r'0x([0-9a-f]{2})',literals))
embedded = []
raw_bytes = 0
for name, number, size in re.findall(r'\{"([^\"]+)", TAB5_RESOURCE_(\d+), sizeof\(TAB5_RESOURCE_\d+\), (\d+)\}', header):
    decoder = zlib.decompressobj()
    unpacked = decoder.decompress(blobs[number]) + decoder.flush()
    assert decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail, name
    assert len(unpacked) == int(size) <= 128*1024, name
    assert unpacked == (sd / name.lstrip('/')).read_bytes(), name
    embedded.append(name)
    raw_bytes += len(unpacked)
assert len(embedded) == len(blobs) == 40
bitmap = (sketch / 'Tab5EnglishBitmap.h').read_text(encoding='utf8')
assert '"-: inward - +: around the edge"' in bitmap, 'Signed feather explanation lost its minus in the bitmap fallback'
assert not any(name.endswith(('.ttf','.elf','.zim','.t5geo')) for name in embedded)
gazetteer = {'System/maps/gazetteer/README.txt', 'System/maps/gazetteer/cities.json', 'System/maps/gazetteer/cities.t5geo'}
assert gazetteer <= manifest['files'].keys()
assert not any('gazetteer/' in name for name in embedded)
assert (sd/'System/maps/gazetteer/cities.t5geo').stat().st_size > 1000000
main = (sketch / (SKETCH+'.ino')).read_text(encoding='utf8')
assert f'APP_VERSION[] = "{VERSION}"' in main
for path in [*sketch.glob('*.cpp'),*sketch.glob('*.h'),*sketch.glob('*.ino')]:
    if path.name in ('Tab5SystemResourcesData.h','Tab5EnglishFallback.h','Tab5EnglishBitmap.h'): continue
    text = path.read_text(encoding='utf8')
    assert 'Tab5Tool::Mail' not in text and 'Tab5Tool::Browser' not in text, path.name
result = {'version':VERSION,'factory_resources':len(manifest['files']),
          'embedded_resources':len(embedded),'rawBytes':raw_bytes,'compressedBytes':sum(map(len,blobs.values())),'python_syntax':True,
          'payload_matches_sd':True,'english_resource_renames':11,'legacy_resource_names_absent':True,'result':'PASS',
          'sourceHashes':{path.relative_to(root).as_posix():hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in [sketch/'Tab5SystemResourcesData.h',
                                       sketch/'Tab5EnglishFallback.h',
                                       sketch/'Tab5EnglishBitmap.h',
                                       sd/f'Factory/Tab5OS-{VERSION}/manifest.json',
                                       Path(__file__).resolve(),
                                       root/'tools/rebuild_factory.py',
                                       root/'tools/generate_system_resources.py',
                                       root/'tools/generate_locale_fallback.py']}}
destination = root / f'tools/verification/resources-{TAG}.json'
destination.parent.mkdir(parents=True,exist_ok=True)
destination.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
print(json.dumps(result))
