import csv
import datetime
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from prepare_m5gfx import prepare_m5gfx
from prepare_audio import prepare_audio
from prepare_lvgl import prepare_lvgl

qa=Path(r'C:\workspace\tab5-qa-0828-clang')
build=Path(r'C:\workspace\tab5-build-0828-clang')
release=Path(__file__).resolve().parents[2]
root=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
name=root.name+'.ino'
result=json.loads((qa/'build-result.json').read_text())
if result.get('exitCode')!=0:raise RuntimeError('The complete compiler must have succeeded first.')
# Recheck original dependency hashes and the complete isolated copies before
# attributing the linked image to these source adaptations.
prepare_m5gfx(Path(r'C:\workspace\tab5-deps-0815\M5GFX'),qa)
prepare_audio(Path(r'C:\workspace\tab5-deps-0815\ESP8266Audio'),qa)
prepare_lvgl(Path(r'C:\workspace\tab5-deps-0815\lvgl'),qa)
dependency_patches=[json.loads((qa/name).read_text()) for name in ['m5gfx-clang-patch.json','audio-clang-patch.json','lvgl-arabic-patch.json']]
initial=json.loads((qa/'source-manifest-start.json').read_text())
final=[]
copy_checks=[]
copy_mismatch=[]
for file in sorted(root.rglob('*')):
    if not file.is_file() or file.relative_to(root).parts[0] == "sdcard":continue
    relative=file.relative_to(root).as_posix()
    data=file.read_bytes()
    final.append({'path':relative,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    copy=build/'sketch'/relative
    if copy.is_file():
        copied=copy.read_bytes()
        if copied.startswith(b'#line 1 '):copied=copied.split(b'\n',1)[1]
        if copied.removeprefix(b'\xef\xbb\xbf')!=data.removeprefix(b'\xef\xbb\xbf'):copy_mismatch.append(relative)
        copy_checks.append(relative)
sources_unchanged=initial['files']==final
(qa/'source-manifest-final.json').write_text(json.dumps({'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'root':str(root),'files':final},indent=2))
if not sources_unchanged:raise RuntimeError('Source manifest changed during the final compilation.')
if copy_mismatch:raise RuntimeError('Arduino source copy mismatches: '+str(copy_mismatch))
sketch=(root/(root.name+'.ino')).read_bytes().removeprefix(b'\xef\xbb\xbf')
sketch_cpp=(build/'sketch'/(name+'.cpp')).read_bytes().removeprefix(b'\xef\xbb\xbf')
sketch_cpp=re.sub(rb'^#line [^\n]*\n',b'',sketch_cpp,flags=re.M)
source_normalized=sketch.replace(b'\r\n',b'\n')
generated_normalized=sketch_cpp.replace(b'\r\n',b'\n')
# Arduino appends one newline when it concatenates the explicitly declared
# sketch. Accept only that exact addition; all source bytes must still match.
arduino_added_newline=generated_normalized==source_normalized+b'\n'
sketch_match=generated_normalized==source_normalized or arduino_added_newline
if not sketch_match:raise RuntimeError('Generated .ino.cpp does not match explicitly declared original sketch.')
elf=build/(name+'.elf')
binary=build/(name+'.bin')
elf_data=elf.read_bytes()
if elf_data[:4]!=b'\x7fELF' or elf_data[4:6]!=b'\x01\x01':raise RuntimeError('Expected ELF32 little endian.')
if struct.unpack_from('<H',elf_data,18)[0]!=243:raise RuntimeError('Expected RISC-V ELF.')
elf_flags=struct.unpack_from('<I',elf_data,36)[0]
if elf_flags&6!=2:raise RuntimeError('Expected single-float RISC-V ABI.')
shoff=struct.unpack_from('<I',elf_data,32)[0]
shentsize,shnum,shstrndx=struct.unpack_from('<HHH',elf_data,46)
rows=[struct.unpack_from('<IIIIIIIIII',elf_data,shoff+i*shentsize) for i in range(shnum)]
strings_row=rows[shstrndx]
strings=elf_data[strings_row[4]:strings_row[4]+strings_row[5]]
sections=[]
for row in rows:
    section_name=strings[row[0]:strings.index(b'\0',row[0])].decode()
    if row[2]&2:
        sections.append({'name':section_name,'address':hex(row[3]),'bytes':row[5],'nobits':row[1]==8})
partitions=[]
for row in csv.reader((build/'partitions.csv').read_text().splitlines()):
    if len(row)>=5 and not row[0].lstrip().startswith('#'):
        partitions.append({'name':row[0].strip(),'type':row[1].strip(),'subtype':row[2].strip(),'offset':int(row[3].strip(),0),'bytes':int(row[4].strip(),0)})
app_sizes=[p['bytes'] for p in partitions if p['type']=='app']
if not app_sizes or binary.stat().st_size>min(app_sizes):raise RuntimeError('Application image exceeds OTA partition.')
esptool=Path(r'C:\Users\belek\AppData\Local\Arduino15\packages\m5stack\tools\esptool_py\5.3.0\esptool.exe')
image=subprocess.run([str(esptool),'--chip','esp32p4','image-info',str(binary)],capture_output=True,text=True,check=True)
(qa/'image-info.log').write_text(image.stdout+image.stderr,encoding='utf-8')
image_info=image.stdout+image.stderr
elf_sha=hashlib.sha256(elf_data).hexdigest()
if 'ESP32-P4' not in image_info or not re.search(r'Checksum:[^\n]*\(valid\)',image_info) or not re.search(r'Validation hash:[^\n]*\(valid\)',image_info):
    raise RuntimeError('Missing expected esptool image validation.')
if '(invalid)' in image_info or elf_sha not in image_info.lower():raise RuntimeError('Firmware integrity or embedded ELF hash mismatch.')
artifacts={}
for suffix in ['.bin','.elf','.map','.merged.bin','.bootloader.bin','.partitions.bin']:
    file=build/(name+suffix)
    if file.is_file():artifacts[suffix]={'file':file.name,'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
log=(qa/'build.log').read_text(errors='replace')
summary=[line for line in log.splitlines() if ('octets' in line or 'bytes' in line) and ('maximum' in line.lower() or 'sketch' in line.lower() or 'variables globales' in line.lower())]
clang=Path(r'C:\Espressif\tools\esp-clang\esp-20.1.1_20250829\esp-clang\bin')
versions={}
for label,command in [
    ('arduino-cli',[r'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe','version']),
    ('clang',[str(clang/'clang.exe'),'--version']),
    ('linker',[str(clang/'riscv32-esp-elf-clang-ld.exe'),'--version'])]:
    version=subprocess.run(command,capture_output=True,text=True,check=True)
    versions[label]=version.stdout.strip()
(qa/'toolchain-versions.json').write_text(json.dumps(versions,indent=2),encoding='utf-8')
evidence={file:hashlib.sha256((qa/file).read_bytes()).hexdigest() for file in ['build.log','compile-args.json','source-manifest-start.json','source-manifest-final.json']}
report={'version':'0.8.28','checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'build':result,
        'toolchain':'Espressif Clang 20.1.1 with Espressif GNU Binutils 2.43.1; Arduino core M5Stack 3.3.9',
        'toolVersions':versions,'evidenceSha256':evidence,'objectFiles':len(list(build.rglob('*.o'))),
        'dependencyPatches':dependency_patches,
        'sourceFilesChecked':len(final),'sourcesUnchanged':sources_unchanged,'arduinoCopiesChecked':len(copy_checks),
        'sourceCopyMismatches':copy_mismatch,'generatedSketchMatches':sketch_match,'arduinoAddedTrailingNewline':arduino_added_newline,'firmwareBytes':binary.stat().st_size,
        'otaPartitionBytes':min(app_sizes),'otaFreeBytes':min(app_sizes)-binary.stat().st_size,
        'imageChip':'ESP32-P4','imageChecksumValid':True,'imageValidationHashValid':True,'elfEmbeddedShaMatches':True,
        'elfFlags':hex(elf_flags),'allocatableSections':sections,'partitions':partitions,'artifacts':artifacts,'cliSummary':summary,
        'physicalDeviceTested':False}
(qa/'firmware-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ['allocatableSections','partitions','artifacts']},indent=2))
