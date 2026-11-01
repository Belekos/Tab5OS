from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parent
names="malloc calloc realloc free memcpy memmove memset memcmp strlen strcmp strncmp strcpy strncpy strchr strrchr strstr strdup fopen fread fwrite fseek ftell fclose feof ferror printf snprintf sprintf vsnprintf puts abort exit".split()
args=[sys.executable,str(root.parent/'build.py'),str(root/'app.c'),'--opt','speed','--math','--include',str(root/'core'),'--extra-source',str(root.parent/'common/runtime.c')]
for p in sorted((root/'core').rglob('*.c')):args+=['--extra-source',str(p)]
for name in names:args+=['--define',name+'=tab5_rt_'+name]
subprocess.run(args+sys.argv[1:],check=True)
