from pathlib import Path
import argparse,hashlib,subprocess,tempfile,shutil
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);a=p.parse_args();sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tab5-shell-',dir=here) as tmp:
 d=Path(tmp)
 for file in ['ShellPlatform.h','shell.cpp']:shutil.copy2(here/file,d/file)
 source=(sketch/'Tab5Shell.cpp').read_text(encoding='utf-8')
 (d/'Tab5Shell.cpp').write_text(source,encoding='utf-8')
 for header in ['unistd.h','dirent.h','fnmatch.h']:(d/header).write_text('#include "ShellPlatform.h"\n',encoding='utf-8')
 subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/W3','/wd4996','/wd4267','/utf-8','/DARDUINO','/DTAB5_SHELL_HOST','/I'+str(d),'/I'+str(sketch),'shell.cpp','/Fe:shell.exe'],cwd=d,check=True)
 subprocess.run([str(d/'shell.exe'),str(d/'volume')],cwd=d,check=True)
 print('Production shell source SHA256:',hashlib.sha256((sketch/'Tab5Shell.cpp').read_bytes()).hexdigest())
