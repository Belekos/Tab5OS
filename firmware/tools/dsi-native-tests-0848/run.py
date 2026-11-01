#!/usr/bin/env python3
"""Run the entire pinned native DW-GDMA driver against controlled HAL/IRQ/heap boundaries."""
import pathlib,subprocess,json,hashlib,argparse,os,sys
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);a=p.parse_args();a.build=a.build.resolve();a.build.mkdir(parents=True,exist_ok=True)
root=pathlib.Path(__file__).resolve().parents[2];test=pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools/build_linux'));from prepare_dsi_cache_safe import patch_sources,UPSTREAM
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();files=[p for p in test.rglob('*') if p.is_file()];files+=[root/'tools/build_linux/prepare_dsi_cache_safe.py',UPSTREAM/'upstream/dw_gdma.c'];before={str(p.relative_to(root)):sha(p) for p in files};results={}
for mode in ('baseline','current'):
 out=a.build/mode;out.mkdir(exist_ok=True)
 source=out/'dw_gdma.c'
 if mode=='current':
  patched=patch_sources();source.write_text(patched['dw_gdma.c']);(out/'tab5_dsi_cache_config.h').write_text(patched['tab5_dsi_cache_config.h'])
 else:source.write_bytes((UPSTREAM/'upstream/dw_gdma.c').read_bytes())
 exe=out/'test'
 cmd=['gcc','-std=gnu17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-Wno-pointer-to-int-cast','-I'+str(test/'stubs'),'-include',str(test/'stubs/platform.h'),'-DEXPECT_SAFE='+str(int(mode=='current')),str(source),str(test/'test.c'),'-o',str(exe)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 results[mode]=json.loads(r.stdout)
assert before=={str(p.relative_to(root)):sha(p) for p in files}
report={'version':'0.8.48','passed':True,'sourceHashes':before,'results':results,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'physicalDeviceTested':False,'boundaries':['Full native dw_gdma.c; actual SDK public types. HAL register effects are controlled stubs.','IRQ masking follows ESP_INTR_FLAG_IRAM policy; this is not a physical cache-off timing test.','Heap capabilities tracked; host allocation sizes/alignment are not P4 SRAM metrics.','Tracked blocks and IRQ handles must be zero after each cycle/fault. LSan disabled due ptrace runtime.']}
out=root/'tools/verification/dsi-native-0848.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
