#!/usr/bin/env python3
"""Render production vector icons at compact/home sizes on actual LVGL9.6."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--lvgl',type=Path,required=True);p.add_argument('--lvgl-build',type=Path,required=True);p.add_argument('--build',type=Path,required=True);a=p.parse_args()
 current=ROOT/'Tab5OS_Arduino_v0.8.53_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True)
 evidence=ROOT/'tools/verification/icons-0853';evidence.mkdir(exist_ok=True)
 initial=sha(current/'Tab5Icons.h');tests={p.relative_to(ROOT).as_posix():sha(p) for p in HERE.iterdir() if p.is_file()}
 results={}
 for mode,source,fixed in [('baseline',a.baseline.resolve(),False),('current',current,True)]:
  cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(source),'-I'+str(a.lvgl.resolve()),str(HERE/'test.cpp'),str(a.lvgl_build.resolve()/'liblvgl.a'),'-o',str(work/mode)]
  if fixed:cmd.insert(1,'-DTAB5_ICONS_FIXED')
  r=subprocess.run(cmd,capture_output=True,text=True);(evidence/(mode+'-compile.log')).write_text(r.stdout+r.stderr)
  if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
  r=subprocess.run([str(work/mode),str(evidence/(mode+'.ppm'))],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
  (evidence/(mode+'-run.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
  if r.returncode:raise SystemExit(r.returncode)
  results[mode]=json.loads(r.stdout.strip().splitlines()[-1])
  from PIL import Image
  Image.open(evidence/(mode+'.ppm')).save(evidence/(mode+'.png'));(evidence/(mode+'.ppm')).unlink()
 assert sha(current/'Tab5Icons.h')==initial
 assert tests=={p.relative_to(ROOT).as_posix():sha(p) for p in HERE.iterdir() if p.is_file()}
 report={'version':'0.8.53','passed':True,'scope':__doc__,'physicalDeviceTested':False,'results':results,
         'sourceHashes':{current.name+'/Tab5Icons.h':initial},'testSourceHashes':tests,
         'evidenceHashes':{p.relative_to(ROOT).as_posix():sha(p) for p in evidence.iterdir() if p.is_file()},
         'sanitizers':['address','undefined'],'lvglArchiveSha256':sha(a.lvgl_build/'liblvgl.a'),
         'limitations':['Software-rendered icons only; no physical Tab5 visibility or touch measurement.','Aesthetic comparison also requires visual inspection of the delivered PNGs.']}
 (ROOT/'tools/verification/icons-0853.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
