#!/usr/bin/env python3
"""Production Writing service/cache lifetime with actual document and index engines."""
from pathlib import Path
import argparse,hashlib,json,os,re,runpy,subprocess,tempfile
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];SKETCH=ROOT/'Tab5OS_Arduino_v0.8.47_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--build',type=Path);a=p.parse_args();build=a.build or Path(tempfile.mkdtemp(prefix='tab5-writing-cache-0847-'));build.mkdir(parents=True,exist_ok=True)
proof=ROOT/'tools/verification/writing-cache-0847';proof.mkdir(parents=True,exist_ok=True)
mask=runpy.run_path(str(ROOT/'tools/audit-0847/inventory_waits.py'))['mask']
def block(text,signature):
 begin=text.index(signature);clean=mask(text);start=clean.index('{',begin);depth=1;end=start+1
 while depth:
  depth+=(clean[end]=='{')-(clean[end]=='}');end+=1
 return text[begin:end]
commands=[];results={};piece_hashes={}
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for name,fixed in [('baseline',False),('current',True)]:
 base=SKETCH if fixed else HERE/'baseline-0846';text=(base/'Tab5Writing.cpp').read_text();fixture=(HERE/'test.cpp').read_text()
 service=block(text,'void Tab5WritingApp::service(');reclaim=block(text,'size_t Tab5WritingApp::reclaimHiddenIndexes(') if fixed else 'size_t Tab5WritingApp::reclaimHiddenIndexes(){return 0;}'
 generated=fixture.replace('/* production_service */',service).replace('/* production_reclaim */',reclaim)
 cpp=build/(name+'.cpp');cpp.write_text(generated)
 source=[SKETCH/n for n in ['WriterDocument.cpp','WriterEncoding.cpp','WriterIndex.cpp','WriterSyntax.cpp','WriterRich.cpp']]+[base/'WriterSyntaxIndex.cpp']
 cmd=['g++','-std=c++17','-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-DARDUINO_ARCH_ESP32','-DTAB5_TEST_FIXED='+str(int(fixed)),'-I'+str(HERE/'stubs'),'-I'+str(base),'-I'+str(SKETCH),str(cpp),*[str(s) for s in source],'-o',str(build/name)]
 commands.append(cmd);compiled=subprocess.run(cmd,text=True,capture_output=True);(proof/(name+'-build.log')).write_text(compiled.stdout+compiled.stderr);compiled.check_returncode()
 result=subprocess.run([str(build/name)],text=True,capture_output=True,env=env);(proof/(name+'-run.log')).write_text(result.stdout+result.stderr);result.check_returncode();results[name]=json.loads(result.stdout)
 piece_hashes[name]={'service':hashlib.sha256(service.encode()).hexdigest(),'reclaim':hashlib.sha256(reclaim.encode()).hexdigest()}
sources=[SKETCH/n for n in ['Tab5Writing.cpp','Tab5Writing.h','WriterIndex.cpp','WriterIndex.h','WriterSyntaxIndex.cpp','WriterSyntaxIndex.h','WriterDocument.cpp','WriterDocument.h','WriterRich.cpp','WriterRich.h','WriterSyntax.cpp','WriterSyntax.h','WriterAllocator.h']]
report={'version':'0.8.47','passed':True,'physicalDeviceTested':False,'baseline':results['baseline'],'fixed':results['current'],'method':__doc__,'productionMethodHashes':piece_hashes,'scope':['Actual production service and reclaimHiddenIndexes methods; unrelated UI/storage/console actions are boundary doubles','Actual Document, RichText, structural DocumentIndex, SyntaxIndex and lexer compiled with PSRAM allocation boundaries','Hidden structural scanning paused and resumes at exact partialposition with4096bytebudget','Only lexical checkpoints reclaimed underpressure; visible/modal/table/resume guards retained','Document bytes, dirty revision, selection, history, undo/redo preserved in100cycles','Optional lexical allocationfailure gracefully handled; all tracked model allocations returned','Autosave disabled does not initiate saves; enabled autosave and previously requested jobs still serviced while hidden'],'limits':['This is not full-OS/LVGL interaction or physicalSD testing','Tracked bytes cover Writer PSRAM allocations; host std::string/vector default allocator excluded','Structural index allocation still uses aborting ExternalAllocator on target; intentionally not reclaimed here','No claim of internal SRAM gain: lexical arrays are PSRAM','LeakSanitizer disabled in environment; ASan and UBSan active'],'sourceHashes':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'testHashes':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in HERE.rglob('*') if p.is_file()},'commands':commands}
(ROOT/'tools/verification/writing-cache-0847.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,**results},indent=2))
