#!/usr/bin/env python3
"""Compile and run the real Tiny386 device adapters (GCC/Clang or MSVC)."""
import argparse,json,os,pathlib,re,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--msvc',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
if a.msvc and a.sanitize: p.error('--sanitize requires GCC or Clang; it is not supported by this MSVC runner')
root=pathlib.Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sources=[root/'tests/test_devices.c']+[root/'core'/n for n in ['ide.c','misc.c','i8257.c','i8042.c','vga.c','vgafont.c','pci.c']]
if a.msvc:
    # MSVC lacks GNU case ranges and POSIX unistd, neither changes semantics.
    (out/'unistd.h').write_text('#pragma once\n')
    for name in ('vga.c','pci.c'):
        portable=out/name
        text=(root/'core'/name).read_text()
        text=re.sub(r'case\s+(0x[0-9a-fA-F]+|\d+)\s*\.\.\.\s*(0x[0-9a-fA-F]+|\d+)\s*:',lambda m:' '.join('case %d:'%i for i in range(int(m[1],0),int(m[2],0)+1)),text)
        portable.write_text(text)
        sources=[portable if s.name==name else s for s in sources]
    exe=out/'devices.exe'
    cmd=['cl','/nologo','/std:c11','/O2','/D_CRT_SECURE_NO_WARNINGS','/DTAB5_DOS386','/DBPP=16','/I'+str(out),'/I'+str(root/'core'),*[str(s) for s in sources],'/Fe:'+str(exe)]
else:
    exe=out/'devices'
    cmd=['cc','-std=gnu11','-O2','-DTAB5_DOS386','-DBPP=16',*[str(s) for s in sources],'-o',str(exe)]
    if a.sanitize: cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
r=subprocess.run(cmd,cwd=out,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/'build.log').write_text(r.stdout)
if r.returncode: print(r.stdout);raise SystemExit(r.returncode)
env=os.environ.copy()
if a.sanitize:
    # Device instances live in the application arena and are reclaimed together
    # by the real loader. This harness intentionally has no object destructors.
    env['ASAN_OPTIONS']='detect_leaks=0'
r=subprocess.run([str(exe)],cwd=out,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);print(r.stdout,end='');(out/'run.log').write_text(r.stdout)
(out/'report.json').write_text(json.dumps({'passed':r.returncode==0,'compiler':cmd[0],'sanitizers':a.sanitize,'output':r.stdout},indent=2)+'\n')
raise SystemExit(r.returncode)
