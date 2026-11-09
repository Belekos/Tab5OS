"""Build the bounded PC/AT native application; only SDK ABI7 is imported."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parent
names='malloc calloc realloc free memcpy memmove memset memcmp strlen strcmp strncmp strcpy strncpy strchr strrchr strstr strdup fopen fread fwrite fseek ftell fclose feof ferror printf snprintf sprintf vsnprintf puts abort exit qsort rand srand time localtime strncasecmp fprintf fflush'.split()
args=[sys.executable,str(r.parent/'build.py'),str(r/'app.c'),'--opt','speed','--include',str(r/'compat'),'--include',str(r),'--define','TAB5_DOS=1','--define','EMUDBG_OFF=1','--define','EMU8950_NATIVE_RATE_ONLY=1','--define','PC_AUDIO_HZ=22050']
for component in ('ibmpc','x86','upd765','diskspace','emudbg','emu8950'):args+=['--include',str(r/'core'/component)]
sources=list((r/'core/ibmpc').glob('*.c'))+[r/'core/x86/x86.c',r/'core/upd765/upd765.c',r/'core/diskspace/fat.c',r/'core/emu8950/emu8950.c',r/'core/emu8950/emuadpcm.c',r/'x87_disabled.c',r/'platform.c',r/'overlay.c',r.parent/'common/runtime.c']
for source in sources:args+=['--extra-source',str(source)]
for name in names:args+=['--define',name+'=tab5_rt_'+name]
subprocess.run(args+sys.argv[1:],check=True)
