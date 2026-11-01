from pathlib import Path
import hashlib,json,os,subprocess,sys
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
build=Path('C:/workspace/tab5-reader-seek-0828');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-reader-0825/lvgl-target.lib')
if not os.environ.get('VSCMD_VER'):
 command=build/'run.cmd';command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_USE_OS=0','/DLV_USE_SPAN=1','/DLV_USE_TINY_TTF=1','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch),'/FI'+str(Path('C:/workspace/tab5-reader-0828/reader-conf.h'))]
r=subprocess.run(['cl',*flags,'/EHsc','/std:c++17',str(Path(__file__).with_name('seek.cpp')),str(library),'/Fe:'+str(build/'seek.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
(build/'build.txt').write_text(r.stdout+r.stderr,encoding='utf-8')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(build/'seek.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace');print(r.stdout+r.stderr)
paths=[sketch/name for name in ['Tab5EpubSeek.inc','Tab5EpubBookIndex.inc','EpubPageIndex.h',sketch.name+'.ino']]+[Path(__file__),Path(__file__).with_name('seek.cpp')]
report={'version':'0.8.28','passed':r.returncode==0,'method':'Production seek controller included directly; real LVGL slider/events/label; controlled document parser, paginator, PSRAM table and bookmark hooks with fault injection. Pagination geometry itself verified separately by reader-layout-0828. No physical tablet.','physicalDeviceTested':False,'output':r.stdout+r.stderr,'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
(release/'tools/verification/reader-seek-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(r.returncode)
