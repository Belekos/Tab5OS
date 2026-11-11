"""Build DOS x86 as an isolated ABI7 application; no firmware replacement."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parent
old=r.parent/'dos'
names='malloc calloc realloc free memcpy memmove memset memcmp strlen strcmp strncmp strcpy strncpy strchr strrchr strstr strdup fopen fread fwrite fseek ftell fclose feof ferror printf snprintf sprintf vsnprintf puts abort exit qsort rand srand time localtime strncasecmp fprintf fflush strtol'.split()
args=[sys.executable,str(r.parent/'build.py'),str(r/'app.c'),'--opt','speed','--math',
      '--include',str(old/'compat'),'--include',str(r),'--include',str(r/'core'),
      '--include',str(old),'--define','TAB5_DOS=1','--define','TAB5_DOS386=1',
      '--define','BPP=16','--define','USE_FAST=1','--define','FAST_PD=1',
      '--define','FAST_PD_SIZE=1048576','--define','I386_ALIGNED_ONLY=1',
      '--define','USE_FMOPL=1']
for feature in ['FPU','MMX','SSE','SSE2','SSE3','SSSE3']:args+=['--define','I386_ENABLE_'+feature+'=1']
for component in ('ibmpc','x86','upd765','diskspace','emudbg'):
 args+=['--include',str(old/'core'/component)]
sources=[r/'core/fast/fast.c']+[r/'core'/name for name in
 ('fpu.c pc.c i8259.c i8254.c ide.c vga.c vgafont.c i8042.c misc.c adlib.c fmopl.c i8257.c sb16.c pcspk.c pci.c').split()]
sources += [r/name for name in ('machine.c platform.c storage.c overlay.c disk_format.c net_disabled.c').split()]
sources += [old/'platform.c',old/'core/ibmpc/folder.c',old/'core/ibmpc/font.c',r.parent/'common/runtime.c']
for source in sources:args+=['--extra-source',str(source)]
for name in names:args+=['--define',name+'=tab5_rt_'+name]
subprocess.run(args+sys.argv[1:],check=True)
