"""Inventory bundled notices and scan release text for concrete secret formats.



No matched value is written to stdout or reports. This is a technical inventory,

not a legal opinion or a guarantee that every possible secret can be detected.

"""

from pathlib import Path

import datetime,hashlib,json,re,struct

from package_release_0830 import SKETCH
PREVIOUS_FACTORY = SKETCH + "/sdcard/Factory/Tab5OS-0.8.29/"

root=Path(__file__).resolve().parents[1]

sketch=next(p for p in root.iterdir() if p.is_dir() and re.fullmatch(r'Tab5OS_Arduino_v\d+\.\d+\.\d+_Ecriture',p.name))

components=[

 ('Draftling Markdown','src/draftling','MIT',['LICENSE']),

 ('MicroPython','src/micropython','MIT',['LICENSE']),

 ('RTL-SDR','src/rtl_sdr','GPL-2.0-or-later; verify notices per file',['COPYING','UPSTREAM.txt']),

 ('libssh2','src/tab5ssh','BSD-3-Clause',['COPYING','UPSTREAM.json','MBEDTLS_BACKPORTS.md']),

 ('libgit2','src/tab5git','GPL-2.0-only WITH linking exception; verify file notices',['COPYING','AUTHORS','UPSTREAM.json']),

 ('WebP','src/tab5webp','BSD-3-Clause',['COPYING']),

 ('Espressif ELF loader','src/tab5_elf','Apache-2.0',['LICENSE','UPSTREAM.md','UPSTREAM.yml']),

 ('Espressif crypto lock compatibility backport','.','Apache-2.0; upstream copyright and source revision retained',['Tab5CryptoLockCompat.c','src/usb_hid/LICENSE']),

 ('Espressif HID host','src/usb_hid','Apache-2.0',['LICENSE']),

 ('Espressif MSC host','src/usb_msc','Apache-2.0',['LICENSE','UPSTREAM.txt']),

 ('XZ Embedded','src/xz','0BSD',['COPYING','UPSTREAM.txt']),

 ('Zstandard','src/zstd','BSD-3-Clause OR GPL-2.0',['LICENSE','UPSTREAM.md']),

 ('JPEG encoder','src/jpeg','upstream public-domain OR Apache-2.0',['PROVENANCE.md','jpge.h','jpge.cpp']),

 ('PNG decoder','src/tab5png','MIT (M5GFX-derived)',['README.md','tab5_pngle.c']),

 ('stb_truetype','src/print','MIT OR public-domain',['stb_truetype.h']),

 ('Peanut GB / MiniGB APU','tools/native-apps/gameboy','MIT',['UPSTREAM.txt','peanut_gb.h','LICENSE-minigb']),

 ('gnuboy','tools/native-apps/gameboy-color/core','GPL-2.0; verify file notices',['COPYING','UPSTREAM.txt']),

 ('Nofrendo','tools/native-apps/nes/core','GPL-2.0; verify file notices',['COPYING','UPSTREAM.txt']),

 ('SMSPlus','tools/native-apps/sega8/core','GPL-2.0; verify file notices',['COPYING','UPSTREAM.txt']),

 ('gpSP','tools/native-apps/gba/core','GPL-2.0; verify file notices',['COPYING','UPSTREAM.txt']),

 ('PrBoom','tools/native-apps/doom/core','GPL-2.0; verify file notices',['COPYING']),

 ('Gwenesis','tools/native-apps/megadrive/core','AGPL-3.0',['LICENSE','UPSTREAM.txt']),

 ('Snes9x/ndssfc','tools/native-apps/snes/core','Mixed: GPL-2.0-or-later and Snes9x non-commercial restriction',['LICENSE','UPSTREAM.txt']),

 ('CJK fonts and input dictionaries','sdcard/System/ime','OFL-1.1; Apache-2.0; GPL-2.0-or-later',['OFL.txt','pinyin-LICENSE.txt','SKK-COPYING.txt','README.md']),

]

rows=[];missing=[]

for name,directory,license_label,notices in components:

 evidence=[]

 for notice in notices:

  path=sketch/directory/notice

  if not path.is_file():missing.append(str(path.relative_to(root)));continue

  evidence.append(dict(path=path.relative_to(root).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest()))

 rows.append(dict(component=name,directory=sketch.name+'/'+directory,licenseLabel=license_label,notices=evidence))

patterns={

 'private-key':re.compile(rb'-----BEGIN (?:RSA |EC |DSA |OPENSSH )?PRIVATE KEY-----'),

 'github-token':re.compile(rb'\b(?:gh[pousr]_[A-Za-z0-9]{36,255}|github_pat_[A-Za-z0-9_]{70,255})\b'),

 'aws-access-key':re.compile(rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b'),

}

findings=[];scanned=0;large=[];scope=[]

for path in root.rglob('*'):

 if not path.is_file():continue

 relative=path.relative_to(root).as_posix()

 if relative.startswith(PREVIOUS_FACTORY):continue

 if any(part in ['.git','__pycache__','build-environment'] for part in path.parts):continue

 if relative.startswith('tools/verification/') or relative=='LICENCES-manifest.json':continue

 scope.append({'path':relative,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})

 if path.stat().st_size>=100*1024*1024:large.append(relative)

 if path.suffix.lower() not in {'.c','.h','.cpp','.inc','.ino','.py','.json','.ini','.md','.txt','.pem','.key','.yml','.yaml','.ps1','.sh','.cmd'}:continue

 if path.name=='audit_publication.py':continue # own literal match definitions

 data=path.read_bytes();scanned+=1

 for kind,pattern in patterns.items():

  for match in pattern.finditer(data):

   # Cryptographic library sources legitimately contain these marker literals,

   # without a following private key. Only complete PEM blocks are findings.

   if kind=='private-key' and not re.search(rb'\n[A-Za-z0-9+/=\r\n]{80,}\n-----END ',data[match.end():match.end()+12000]):continue

   findings.append(dict(path=relative,line=data[:match.start()].count(b'\n')+1,kind=kind))

font=sketch/'sdcard/System/font.ttf';font_names=[]

if font.is_file():

 data=font.read_bytes();count=struct.unpack_from('>H',data,4)[0]

 for at in range(12,12+16*count,16):

  tag,checksum,offset,length=struct.unpack_from('>4sIII',data,at)

  if tag!=b'name':continue

  _,n,strings=struct.unpack_from('>HHH',data,offset)

  for entry in range(offset+6,offset+6+12*n,12):

   platform,encoding,language,name_id,size,start=struct.unpack_from('>HHHHHH',data,entry)

   if name_id not in {1,13,14}:continue

   raw=data[offset+strings+start:offset+strings+start+size]

   try:value=raw.decode('utf-16-be' if platform in (0,3) else 'mac_roman')

   except UnicodeError:continue

   row=dict(id=name_id,text=value)

   if row not in font_names:font_names.append(row)

manifest=dict(version='0.8.30',generatedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='Bundled source notices and SD resources. External Arduino libraries remain recorded in BIBLIOTHEQUES.md/toolchain proofs.',firstPartyLicense='Not selected by the project owner',components=rows,primaryFontNames=font_names,missingNotices=missing)

(root/'LICENCES-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

report=dict(version='0.8.30',checkedAt=manifest['generatedAt'],passed=not missing and not findings and not large,physicalDeviceTested=False,licenseDecisionRequired=True,missingNotices=missing,secretScan={'scannedTextFiles':scanned,'findings':findings,'matchedValuesDisclosed':False},githubFilesAtLeast100MiB=large,limitations=['Pattern scan is not exhaustive','No principal license applied without owner choice','Snes9x non-commercial notice prevents describing every bundled component as permissively licensed'])

scope.sort(key=lambda row:row['path'])

scope_path=root/'tools/verification/publication-scope-0830.json'

scope_path.write_text(json.dumps({'files':scope,'excluded':['.git','__pycache__','build-environment','tools/verification','LICENCES-manifest.json',PREVIOUS_FACTORY]},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

report['scope']={'manifest':'tools/verification/publication-scope-0830.json','sha256':hashlib.sha256(scope_path.read_bytes()).hexdigest(),'fileCount':len(scope)}

report['auditScriptSha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()

(root/'tools/verification/publication-0830.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

print(json.dumps({'passed':report['passed'],'components':len(rows),'missingNotices':missing,'secretFindings':len(findings),'largeFiles':len(large),'fontFamilies':[n['text'] for n in font_names if n['id']==1]},ensure_ascii=False))

raise SystemExit(0 if report['passed'] else 1)

