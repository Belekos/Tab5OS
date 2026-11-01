"""Check all reviewed image-editor string literals against the 14 language packs.

Includes, paths, file magic and purely numeric formatting are explicit exclusions.
This bounded scan deliberately fails on a new unreviewed literal rather than
assuming that all strings outside direct tr() calls are untranslatable.
"""
from pathlib import Path
import json,re,hashlib
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.34_Ecriture'
SOURCES=['Tab5ImageEditor.cpp','Tab5ImageCodec.cpp','tools/native-apps/image-editor/image_core.c']
EXCLUDED={'.bmp','.t5i','.jpg','.jpeg','T5IMG001',
 ' · (%d, %d) → (%d, %d) · %d × %d · %s %.1f · %s %.1f°',
 '%d × %d · %d%% · %s %d/%d · %s %u/%u%s%s%s%s','%d%%','%d','#%06X','%s%d · %s',
 # Gesture footer: first %s is tr("Historique"), last %s already contains
 # translated metric labels. This format contains no untranslated words.
 '%s %u/%u%s'}
report_path=ROOT/'tools/verification/drawing-localization-0834.json'
report=json.loads(report_path.read_text())
packs={p.stem:json.loads(p.read_text())['translations'] for p in (SKETCH/'sdcard/System/languages').glob('*.json')}
keys=set();source_hashes={};unknown=[]
for name in SOURCES:
 p=SKETCH/name;s=p.read_text()
 source_hashes[str(p.relative_to(ROOT))]=hashlib.sha256(p.read_bytes()).hexdigest()
 # Remove comments while preserving string tokens.
 tokens=re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:[^"\\]|\\.)*"',s)
 for m in tokens:
  if not m.group().startswith('"'):continue
  value=json.loads(m.group())
  if not value or not re.search('[A-Za-zÀ-ÿ]',value):continue
  if value in EXCLUDED or value.endswith('.h') or value.startswith('/Images'):continue
  keys.add(value)
  if value not in packs['en-US']:unknown.append((name,value))
assert not unknown,unknown
for locale,pack in packs.items():
 missing=keys-pack.keys();assert not missing,(locale,sorted(missing))
for name in ['drawing-ui-delta-0834.json','drawing-diagnostics-delta-0834.json','drawing-delta-0834.json','build_drawing_delta_0834.py','build_drawing_diagnostics_0834.py','apply_drawing_0834.py','audit_drawing_keys_0834.py']:
 p=HERE/name;source_hashes[str(p.relative_to(ROOT))]=hashlib.sha256(p.read_bytes()).hexdigest()
report['sourceHashes']=source_hashes
report['sourceKeyCoverage']={'passed':True,'translatedKeysInImageSources':len(keys),'missingKeys':[],'method':'All reviewed literals in editor, codec and core; explicit exclusion of includes, paths, file magic and numeric format strings.'}
report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('PASS',len(keys),'image source keys covered by',len(packs),'packs')
