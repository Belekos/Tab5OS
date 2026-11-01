#!/usr/bin/env python3
"""Run production ELF editor UI/core and validate/rebuild the delivered RISC-V ELF."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile
from PIL import Image
here=Path(__file__).resolve().parent
release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.32_Ecriture'
app=sketch/'tools/native-apps/image-editor'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--toolchain',type=Path,default=Path(os.environ['ESP_RV32_BIN']) if 'ESP_RV32_BIN' in os.environ else None)
a=parser.parse_args()
if not a.toolchain:parser.error('--toolchain is required (Espressif GCC14.2.0 bin directory)')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(command,**kwargs):return subprocess.run([str(x) for x in command],check=True,text=True,capture_output=True,**kwargs)
sources=[*sorted(p for p in app.iterdir() if p.is_file()),sketch/'Tab5AppApi.h',sketch/'Tab5ElfValidate.cpp',sketch/'Tab5ElfValidate.h',sketch/'tools/native-apps/build.py',sketch/'sdcard/Applications/AtelierImage.app.elf.launch.json',sketch/'sdcard/Images/LISEZ-MOI.txt',here/'run.py',here/'validate.cpp']
before={str(p.relative_to(release)):sha(p) for p in sources}
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer']
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
proof=release/'tools/verification/image-editor-build-0832';proof.mkdir(parents=True,exist_ok=True)
shots=release/'tools/verification/image-editor-ui-0832';shots.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tab5-image-ui-') as work:
 work=Path(work)
 run(['cc',*flags,'-I',sketch,app/'test_ui.c',app/'image_core.c','-o',work/'ui'])
 r=run([work/'ui',work],env=env);ui=json.loads(r.stdout.strip().splitlines()[-1]);print(r.stdout,end='')
 for p in work.glob('*.ppm'):Image.open(p).save(shots/p.with_suffix('.png').name)
 r=run(['python3',app/'run_tests.py']);print(r.stdout,end='')
 run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-I',sketch,here/'validate.cpp',sketch/'Tab5ElfValidate.cpp','-o',work/'validate'])
 elf=sketch/'sdcard/Applications/AtelierImage.app.elf'
 r=run([work/'validate',elf],env=env);validated=json.loads(r.stdout);print(r.stdout,end='')
 rebuilt=work/'AtelierImage.app.elf'
 command=['python3',app/'build.py','--toolchain',a.toolchain,'-o',rebuilt]
 r=run(command);(proof/'rebuild.log').write_text(r.stdout+r.stderr)
 assert sha(rebuilt)==sha(elf),'Rebuilt ELF differs from delivered binary'
 readelf=a.toolchain/'riscv32-esp-elf-readelf'
 headers=run([readelf,'-h',elf]).stdout;symbols=run([readelf,'-Ws',elf]).stdout
 imports=sorted({line.split()[-1] for line in symbols.splitlines() if ' UND ' in line and len(line.split())>=8})
 assert imports==['tab5_app_api_v6']
 assert 'ELF32' in headers and 'RISC-V' in headers and 'DYN (Shared object file)' in headers
 assert 'tab5_app_main' in symbols
 (proof/'elf-headers.txt').write_text(headers);(proof/'elf-symbols.txt').write_text(symbols)
 target=run([a.toolchain/'riscv32-esp-elf-gcc','--version']).stdout.splitlines()[0]
 core=json.loads((release/'tools/verification/image-core-0832.json').read_text())
 assert core['status']=='passed' and all(sha(release/p)==h for p,h in core['sourceHashes'].items())
 after={str(p.relative_to(release)):sha(p) for p in sources};assert before==after
 report={'version':'0.8.32','passed':True,'component':'image-editor-native-elf','physicalDeviceTested':False,'sourceStableDuringTests':True,'sourceHashes':after,'ui':ui,'core':{'checks':core['checks'],'repeatedCycles':core['repeated_cycles'],'allocationScenarios':core['allocation_scenarios'],'report':'tools/verification/image-core-0832.json','reportSha256':sha(release/'tools/verification/image-core-0832.json')},'elf':{'path':str(elf.relative_to(release)),'bytes':elf.stat().st_size,'sha256':sha(elf),'abi':6,'format':'ELF32 little-endian RISC-V ET_DYN','entry':'tab5_app_main','imports':imports,'validator':validated,'rebuildByteIdentical':True,'compiler':target,'command':[str(c) for c in command]},'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'leakValidation':'LSan disabled because sandbox /proc task introspection is unavailable; production chunk allocator + SDK allocation double assert zero bytes/allocations at exit, core asserts zero after every scenario.','coverage':['Actual app UI touch/key/dialog code, 39 rendered frames','Stroke cancellation, Ctrl+Z/Y mid-gesture, lost release/new press','Layer creation/duplication/order/deletion, opacity drag cancellation','Selection/crop, text UTF8 input/backspace, Enter/Escape','T5I save/open, overwrite confirmation, overlong path rejection','New/Open/Quit dirty confirmation and cancellation','Actual chunk allocator near16MiB cap and return of free chunks during app use','Pure production ELF validator checks delivered P4 executable, then byte-identical rebuild'],'limitations':['Host UI uses mocked SDK input and SD callbacks; actual hardware/touch/SD performance not tested.','Only BMP24 round-trip fixture executed; BMP32 BI_RGB reader implemented but no specific32-bit fixture in this report.','No PNG/JPEG support; transformations use nearest neighbour and quarter-turn rotations.'],'evidenceHashes':{str(p.relative_to(release)):sha(p) for p in sorted(proof.iterdir()) if p.is_file()},'screenshots':{str(p.relative_to(release)):sha(p) for p in sorted(shots.glob('*.png'))}}
 out=release/'tools/verification/image-editor-0832.json';out.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n');print(out)
