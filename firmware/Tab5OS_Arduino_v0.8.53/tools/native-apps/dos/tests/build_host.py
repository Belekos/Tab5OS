"""Build the real headless PC core for a host boot/COM/MZ regression test."""
from pathlib import Path
import argparse, subprocess
p=argparse.ArgumentParser();p.add_argument('--zig');p.add_argument('--msvc',action='store_true');p.add_argument('--cc',default='cc');p.add_argument('--main',type=Path);p.add_argument('--keyboard-source',type=Path);p.add_argument('-o',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1]
cmd=(['cl','/nologo','/std:c11','/O2','/D_CRT_SECURE_NO_WARNINGS','/DEMUDBG_OFF','/FI'+str(r/'tests/host-compat/platform.h'),'/I'+str(r/'tests/host-compat')] if a.msvc else ([a.zig,'cc'] if a.zig else [a.cc])+['-std=gnu11','-O2','-DEMUDBG_OFF','-include',str(r/'tests/host-compat/platform.h')])
for define in ('EMU8950_NATIVE_RATE_ONLY=1','PC_AUDIO_HZ=22050'):cmd+=[('/D' if a.msvc else '-D')+define]
for component in ('ibmpc','x86','upd765','diskspace','emudbg','emu8950'):cmd+=[('/I' if a.msvc else '-I'),str(r/'core'/component)]
sources=list((r/'core/ibmpc').glob('*.c'))+[r/'core/x86/x86.c',r/'core/upd765/upd765.c',r/'core/diskspace/fat.c',r/'core/emu8950/emu8950.c',r/'core/emu8950/emuadpcm.c',r/'x87_disabled.c',a.main.resolve() if a.main else r/'tests/boot_host.c']
if a.keyboard_source:sources=[a.keyboard_source.resolve() if s==r/'core/ibmpc/keyboard.c' else s for s in sources]
cmd += [str(s) for s in sources]+(['/Fe:'+str(a.o)] if a.msvc else ['-lm','-o',str(a.o)])
a.o.parent.mkdir(parents=True,exist_ok=True);print('Compiling real PC core',flush=True);subprocess.run(cmd,check=True,cwd=a.o.parent)
