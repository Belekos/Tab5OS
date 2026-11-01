from pathlib import Path
import datetime,hashlib,json,os,runpy,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
build=Path('C:/workspace/tab5-writing-recovery-0828');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
 script=build/'run.cmd';script.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n','utf8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(script)]))
# Reuse only the controller extractor, without executing the UI test runner.
extract=(release/'tools/writing-serial-tests-0828/run.py').read_text('utf8');scope={};exec(extract[extract.index('def block('):extract.index("if not os.environ.get('VSCMD_VER')")],scope);block=scope['block']
source=(sketch/'Tab5Writing.cpp').read_text('utf8');test=(here/'test.cpp').read_text('utf8')
signatures=['void serviceRecoverySave()','void Tab5WritingApp::requestRecoverySave()','Tab5WritingApp::RecoverySaveState Tab5WritingApp::recoverySaveState()','const char* Tab5WritingApp::recoverySavePath()','const char* Tab5WritingApp::recoverySaveError()','void Tab5WritingApp::cancelRecoverySave()']
for signature in signatures:test=test.replace('/* '+signature+' */',block(source,signature))
assert 's.serviceRecoverySave();' in block(source,'void Tab5WritingApp::service(')
cpp=build/'recovery.cpp';cpp.write_text(test,'utf8')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sources={name:sha(sketch/name) for name in ['Tab5Writing.cpp','Tab5Writing.h','WriterStorage.cpp','WriterStorage.h','WriterDocument.cpp','WriterDocument.h']}
cmd=['cl','/nologo','/utf-8','/EHsc','/std:c++17','/Zi','/fsanitize=address','/DLV_CONF_SKIP','/D_CRT_SECURE_NO_WARNINGS','/IC:/workspace/tab5-deps-0815/lvgl','/IC:/workspace/tab5-deps-0815/lvgl/include','/I'+str(sketch),str(cpp),str(sketch/'WriterStorage.cpp'),str(sketch/'WriterDocument.cpp'),'/Fe:'+str(build/'recovery.exe')]
r=subprocess.run(cmd,cwd=build,capture_output=True);(build/'compile.log').write_bytes(r.stdout+r.stderr)
if r.returncode:print((r.stdout+r.stderr).decode('utf8','replace'));raise SystemExit(r.returncode)
r=subprocess.run([str(build/'recovery.exe')],cwd=build,capture_output=True,timeout=90);output=(r.stdout+r.stderr).decode('utf8','replace');print(output)
stable=sources=={n:sha(sketch/n) for n in sources}
report=dict(version='0.8.28',passed=r.returncode==0 and stable,physicalDeviceTested=False,sourceStableDuringTests=stable,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),sources=sources,tests={p.name:sha(p) for p in here.glob('*') if p.is_file()},extractorSha256=sha(release/'tools/writing-serial-tests-0828/run.py'),extractorIdentity='Entire writing-serial-tests-0828/run.py file; only its block() definition is executed.',method='Actual recovery controllers and production SaveJob/Document under MSVC AddressSanitizer; filesystem faults are explicit boundary doubles. Bounded data steps, SD failures, transaction preservation, cancel deadline, copy snapshot and existing save wait are exercised.',exitCode=r.returncode,output=output)
(release/'tools/verification/writing-recovery-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n','utf8');raise SystemExit(0 if report['passed'] else 1)
