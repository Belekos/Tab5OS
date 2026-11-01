from pathlib import Path
import datetime,hashlib,json,subprocess,re
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture';tests=Path(__file__).parent
build=Path('C:/workspace/tab5-battery-integration-0828');build.mkdir(exist_ok=True)
def block(text,signature):
 begin=text.index(signature);start=text.index('{',begin);depth=0;quote=None;escape=False;line=False;comment=False;i=start
 while i<len(text):
  c=text[i];n=text[i:i+2]
  if line:
   if c=='\n':line=False
  elif comment:
   if n=='*/':comment=False;i+=1
  elif quote:
   if escape:escape=False
   elif c=='\\':escape=True
   elif c==quote:quote=None
  elif n=='//':line=True;i+=1
  elif n=='/*':comment=True;i+=1
  elif c in "\"'":quote=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[begin:i+1]
  i+=1
 raise ValueError(signature)
paths=[sketch/'Tab5OS_Arduino_v0.8.28_Ecriture.ino',sketch/'Tab5Tools.cpp',sketch/'Tab5BatteryGuard.h',sketch/'Tab5BatteryUi.inc',tests/'firmware.cpp',tests/'run_integration.py']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={str(p.relative_to(release)).replace('\\','/'):sha(p) for p in paths}
source=(sketch/'Tab5Tools.cpp').read_text('utf8');main=paths[0].read_text('utf8');loop=block(main,'void loop()')
assert '#include "Tab5BatteryGuard.h"' in main and '#include "Tab5BatteryUi.inc"' in main
assert loop.count('serviceBatteryProtection(')==1
assert loop.index('Tab5Writing.service(')<loop.index('serviceBatteryProtection(')
assert '#include "Tab5BatteryGuard.h"' in source
pieces={'enabled':block(source,'static void enabled('),'build':block(source,'if(which==Tab5Tool::Firmware){'),'act':block(source,'if(tool==Tab5Tool::Firmware){\n   if((action'),'service':block(source,'if(a->tool==Tab5Tool::Firmware){')}
generated=(tests/'firmware.cpp').read_text('utf8')
for key,value in pieces.items():generated=generated.replace('/* '+key+' */',value)
cpp=build/'firmware.cpp';cpp.write_text(generated,'utf8')
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0826-host/lvgl.lib');vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
args=f'"{vc}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /Od /Zi /MT /fsanitize=address /DLV_CONF_SKIP /DLV_USE_STDLIB_MALLOC=1 /DLV_LABEL_TEXT_SELECTION=1 /DLV_USE_OS=0 /I"{lvgl}" /I"{lvgl/"include"}" /I"{sketch}" "{cpp}" "{library}" /Fe:"{build/"firmware.exe"}" /Fo:"{build}/" /Fd:"{build/"firmware.pdb"}"'
compiled=subprocess.run('cmd.exe /d /s /c "'+args+'"',capture_output=True,text=True,encoding='utf-8',errors='replace');print(compiled.stdout+compiled.stderr)
result=subprocess.run([str(build/'firmware.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60) if compiled.returncode==0 else None
output=(result.stdout+result.stderr) if result else '';print(output)
stable=before=={str(p.relative_to(release)).replace('\\','/'):sha(p) for p in paths}
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),method='Exact Tools firmware construction/actions/service branches and enabled helper on real LVGL9.6; firmware/job boundaries mocked. Main sketch call-order integration assertion. MSVC ASan; prebuilt LVGL archive uninstrumented.',physicalDeviceTested=False,sourceStableDuringTests=stable,passed=stable and compiled.returncode==0 and result is not None and result.returncode==0,checks=sum(map(int,re.findall(r'PASS (\d+) checks',output))),compileOutput=compiled.stdout+compiled.stderr,output=output,lvglArchiveSha256=sha(library),sourceHashes=before,controllerHashes={k:hashlib.sha256(v.encode('utf8')).hexdigest() for k,v in pieces.items()})
(release/'tools/verification/battery-integration-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n','utf8')
raise SystemExit(0 if report['passed'] else 1)
