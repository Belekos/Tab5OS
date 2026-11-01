#!/usr/bin/env python3
"""Validate production font selection/loader with host I/O doubles, and shipped TTFs with real LVGL."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parent.parent;source=root/'Tab5OS_Arduino_v0.8.39_Ecriture';build=a.build.resolve();build.mkdir(parents=True,exist_ok=True)
proof=root/'tools/verification/font-defaults-0839';proof.mkdir(parents=True,exist_ok=True)
font_paths=[source/'sdcard/System/font.ttf']+[source/'sdcard/System/fonts'/f for f in ('bold.ttf','italic.ttf','bolditalic.ttf')]
production=[source/f for f in ('Tab5FontDefaults.h','Tab5FontDefaults.cpp','Tab5FontDefaultsValidation.cpp')]+font_paths
harness=[p for p in sorted(here.iterdir()) if p.is_file()]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def hashes(paths):return {p.relative_to(root).as_posix():sha(p) for p in paths}
before=hashes(production+harness)
flags=['-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(source),'-I'+str(here)]
validation=source/'Tab5FontDefaultsValidation.cpp'
commands={
 'structure':['g++',*flags,str(here/'font_defaults_test.cpp'),str(validation),'-o',str(build/'structure')],
 'loader':['g++',*flags,'-DTAB5_FONT_DEFAULTS_HOST_TEST',str(here/'font_loader_test.cpp'),str(validation),str(source/'Tab5FontDefaults.cpp'),'-o',str(build/'loader')],
 'lvgl':['g++',*flags,'-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include'),str(here/'font_lvgl_test.cpp'),str(validation),str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(build/'lvgl')]
}
results={};env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for name,cmd in commands.items():
 b=subprocess.run(cmd,capture_output=True,text=True);(proof/(name+'-build.log')).write_text(b.stdout+b.stderr)
 if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
 args=font_paths if name=='structure' else [font_paths[0]] if name=='loader' else [font_paths[0],font_paths[2]]
 t=subprocess.run([str(build/name),*map(str,args)],capture_output=True,text=True,env=env,timeout=180);log=t.stdout+t.stderr;(proof/(name+'-run.log')).write_text(log);print(log,end='')
 if t.returncode:raise SystemExit(t.returncode)
 results[name]={'passed':True,'checks':int(re.search(r'PASS (\d+)',log).group(1))}
after=hashes(production+harness);assert before==after,'Source changed during tests'
report={'version':'0.8.39','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':True,'checks':sum(v['checks'] for v in results.values()),'results':results,'sourceHashes':hashes(production),'testSourceHashes':hashes(harness),'evidenceHashes':hashes(list(sorted(proof.iterdir()))),'lvglArchiveSha256':sha(a.lvgl_build/'liblvgl.a'),'sanitizers':['address','undefined'],'method':__doc__,'limits':{'perFontBytes':6*1024*1024,'customTotalBytes':8*1024*1024,'readChunkBytes':16384,'outlineCompositeDepth':32},'limitations':['Filesystem, PSRAM allocation and memory admission are host doubles; no physical SD or ESP32-P4 timing measured.','Real LVGL test exercises TTF creation, multilingual fallback and coordinated zoom independently of the settings UI integration.','Structural validation accepts supported static TrueType outlines; it is not a complete OpenType conformance/security validator.','LeakSanitizer is disabled because this executor cannot inspect /proc task metadata; allocation tracking verifies loader ownership and release.']}
(root/'tools/verification/font-defaults-0839.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
