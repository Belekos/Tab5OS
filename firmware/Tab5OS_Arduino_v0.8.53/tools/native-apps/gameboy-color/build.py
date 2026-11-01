from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parent
names='malloc calloc free memcpy memset memcmp strlen strcmp strncmp fopen fread fwrite fseek fclose feof'.split()
args=[sys.executable,str(root.parent/'build.py'),str(root/'app.c'),'--opt','speed']
for p in sorted((root/'core').glob('*.c')):args+=['--extra-source',str(p)]
for name in names:args+=['--define',name+'=tab5_gbc_'+name]
subprocess.run(args+sys.argv[1:],check=True)
