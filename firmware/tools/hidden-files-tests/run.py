"""Exercise actual visibility preferences and JSON parser with fault-injected storage."""
from pathlib import Path
import argparse, shutil, subprocess, tempfile
p=argparse.ArgumentParser()
p.add_argument('sketch',type=Path)
p.add_argument('--json-include',type=Path,default=Path(r'C:\workspace\tab5-deps-0815\ArduinoJson\src'))
p.add_argument('--cxx',default='cl')
a=p.parse_args()
here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tab5-hidden-') as name:
 d=Path(name)
 for f in here.glob('*.h'):shutil.copy2(f,d/f.name)
 for f in ['Tab5HiddenFiles.h','Tab5FilePolicy.h','Tab5JsonFile.h']:shutil.copy2(a.sketch/f,d/f)
 shutil.copy2(here/'main.cpp',d/'main.cpp')
 subprocess.run([a.cxx,'/nologo','/std:c++17','/EHsc','/utf-8','/I.', '/I'+str(a.json_include.resolve()),'main.cpp','/Fe:test.exe'],cwd=d,check=True)
 subprocess.run([str(d/'test.exe')],cwd=d,check=True)
