from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parent
names="malloc calloc realloc free memcpy memmove memset memcmp strlen strcmp strncmp strcpy strncpy strchr strrchr strstr strdup fopen fread fwrite fseek ftell fclose feof ferror printf snprintf sprintf vsnprintf puts abort exit qsort rand srand time localtime strncasecmp".split()
args=[sys.executable,str(root.parent/'build.py'),str(root/'app.c'),'--opt','speed','--math','--define','GWENESIS_AUDIO_ACCURATE=1','--include',str(root/'compat'),'--extra-source',str(root.parent/'common/runtime.c'),'--extra-source',str(root.parent/'common/math.c')]
for part in ['bus','cpus/M68K','cpus/Z80','io','savestate','sound','vdp']:args+=['--include',str(root/'core/src'/part)]
for p in sorted((root/'core').rglob('*.c')):
 if p.name in ['ConDebug.c','Debug.c','gen68k_adapter.c']:continue
 args+=['--extra-source',str(p)]
for name in names:args+=['--define',name+'=tab5_rt_'+name]
subprocess.run(args+sys.argv[1:],check=True)
