"""Run production context menu, focus, editor and style against actual LVGL."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys

release = Path(__file__).resolve().parents[2]
version = '0.8.26'
sketch = release / ('Tab5OS_Arduino_v' + version + '_Ecriture')
lvgl = Path('C:/workspace/tab5-deps-0815/lvgl')
build = Path('C:/workspace/tab5-context-0826-host')
build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command = build / 'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"' + sys.executable + '" -X utf8 "' + str(Path(__file__).resolve()) + '"\n', encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd', '/d', '/c', str(command)]))

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

names = ['Tab5ContextMenu.cpp','Tab5ContextMenu.h','Tab5TextInput.cpp','Tab5TextInput.h',
         'Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterKeys.h',
         'WriterDocument.cpp','WriterDocument.h']
sources = {name:digest(sketch/name) for name in names}
tests = {name:digest(Path(__file__).with_name(name)) for name in ('run.py','context_menu.cpp')}
flags = ['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1',
         '/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
library = Path('C:/workspace/tab5-context-0823-host/lvgl.lib')
stamp = library.with_name('lvgl-source.json')
inputs = {p.relative_to(lvgl).as_posix():digest(p) for p in sorted(lvgl.rglob('*')) if p.is_file() and p.suffix in ('.c','.h')}
expected = {'sources':inputs,'flags':flags[:8]}

def run(args, log):
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=build,stdout=stream,stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(encoding='utf-8',errors='replace')[-18000:])
        raise SystemExit(result.returncode)

if not library.exists() or not stamp.exists() or json.loads(stamp.read_text()) != expected:
    print('Building LVGL 9.6.0 from source',flush=True)
    units=[p for p in (lvgl/'src').rglob('*.c') if p.name!='vg_lite_matrix.c']
    rsp=build/'lvgl.rsp'
    rsp.write_text('\n'.join('"'+str(arg)+'"' for arg in flags+['/std:c11','/MP8','/c']+[str(p) for p in units]),encoding='utf-8')
    run(['cl','@'+str(rsp)],build/'lvgl-build.log')
    rsp.write_text('\n'.join('"'+str(p)+'"' for p in build.glob('*.obj')),encoding='utf-8')
    run(['lib','/nologo','/OUT:'+str(library),'@'+str(rsp)],build/'lvgl-link.log')
    stamp.write_text(json.dumps(expected,indent=2),encoding='utf-8')
run(['cl',*flags,'/EHsc','/std:c++17',str(Path(__file__).with_name('context_menu.cpp')),str(sketch/'WriterDocument.cpp'),str(sketch/'Tab5Focus.cpp'),str(library),'/Fe:'+str(build/'context_menu.exe')],build/'build.log')
result=subprocess.run([str(build/'context_menu.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8')
output=result.stdout+result.stderr
print(output,flush=True)
match=re.search(r'PASS (\d+) context-menu checks; groups=(\d+); cycles=(\d+); history_bytes=(\d+); LVGL=(\S+);',output)
stable=sources=={name:digest(sketch/name) for name in names}
report=dict(version=version,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=result.returncode==0 and bool(match) and stable,
            physicalDeviceTested=False,exitCode=result.returncode,output=output,sources=sources,tests=tests,
            sourceStableDuringTests=stable,method='MSVC host execution of production context menu, focus, text editor and style with LVGL compiled from source',
            cases=re.findall(r'^CASE (.+)$',output,re.M),checks=int(match[1]) if match else 0,groups=int(match[2]) if match else 0,
            cycles=int(match[3]) if match else 0,historyBytes=int(match[4]) if match else None,
            lvglVersion=match[5] if match else None,lvglLibrarySha256=digest(library),lvglSourceManifest=expected,
            limitations=['Mouse clicks are real LVGL event dispatches; USB mouse transport is tested separately.',
                         'OOM injection covers the nothrow OutputBinding allocation only; LVGL allocator exhaustion is not claimed.',
                         'historyBytes measures production editing snapshots, not all host or Tab5 heap allocations.'])
(release/'tools/verification/context-menu-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
