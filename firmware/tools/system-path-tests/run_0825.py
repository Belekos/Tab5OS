"""Real System aliases, extracted service/import converters and VolumeFS routing."""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);a=p.parse_args()
sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
def extract(text,name):
 m=re.search(r'^(?:static )?(?:int|bool|mp_import_stat_t) '+name+r'\([^;{}]*\)\s*\{',text,re.M);assert m,name
 depth=1
 for t in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\{|\}',text[m.end():],re.S):
  if t.group()=='{':depth+=1
  elif t.group()=='}':
   depth-=1
   if not depth:return text[m.start():m.end()+t.end()]
 raise ValueError(name)
sources=['Tab5SystemPath.h','Tab5VolumeFS.h','Tab5VolumeFS.cpp','Tab5Storage.h','Tab5Storage.cpp','Tab5FilePolicy.h','Tab5PythonServices.cpp','Tab5PythonArchives.cpp','Tab5Python.cpp','src/micropython/tab5_module.c','src/micropython/py/reader.c']
with tempfile.TemporaryDirectory(prefix='tab5-system-paths-') as tmp:
 d=Path(tmp)
 for src in (here.parent/'system-resources-tests',here.parent/'system-resources-tests/volume',here):
  for f in src.glob('*'):
   if f.suffix in ('.h','.cpp','.c'):shutil.copy2(f,d/f.name)
 for name in sources:
  if '/' not in name and name not in ('Tab5PythonServices.cpp','Tab5PythonArchives.cpp','Tab5Python.cpp'):shutil.copy2(sketch/name,d/name)
 (d/'service_path.inc').write_text(extract((sketch/'Tab5PythonServices.cpp').read_text(encoding='utf-8'),'path'),encoding='utf-8')
 (d/'archive_path.inc').write_text(extract((sketch/'Tab5PythonArchives.cpp').read_text(encoding='utf-8'),'sdPath'),encoding='utf-8')
 mp=(sketch/'src/micropython/tab5_module.c').read_text(encoding='utf-8')
 (d/'mp_import.inc').write_text('\n'.join(extract(mp,n) for n in ('mp_import_stat','tab5_mp_open_import')),encoding='utf-8')
 assert 'tab5SystemPythonPath(full,sizeof(full),path)' in mp
 reader=(sketch/'src/micropython/py/reader.c').read_text(encoding='utf-8')
 assert 'tab5_mp_open_import(qstr_str(filename))' in reader
 commands=['cl /nologo /utf-8 /EHsc /std:c++17 /I. test_paths.cpp /Fe:paths.exe','paths.exe','cl /nologo /utf-8 /TC /std:c17 /I. test_header.c /Fe:header.exe','header.exe','cl /nologo /utf-8 /EHsc /std:c++17 /I. Tab5VolumeFS.cpp Tab5Storage.cpp test_volume.cpp /Fe:volume.exe','volume.exe']
 script='@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\n'+''.join(command+'\nif not "%errorlevel%"=="0" exit /b 1\n' for command in commands)
 (d/'run.cmd').write_text(script,encoding='utf-8')
 r=subprocess.run(['cmd','/d','/c',str(d/'run.cmd')],cwd=d,capture_output=True,text=True,encoding='utf-8',errors='replace');print(r.stdout+r.stderr)
 report=dict(version='0.8.25',passed=r.returncode==0,exitCode=r.returncode,physicalDeviceTested=False,method='MSVC C17/C++17 production path helpers, native import/service/archive function bodies, actual VolumeFS and Storage with in-memory VFS and RTOS boundaries. No physical SD tested.',output=r.stdout+r.stderr,sources={name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in sources},tests={str(f.relative_to(here)):hashlib.sha256(f.read_bytes()).hexdigest() for f in here.iterdir() if f.is_file()})
 (sketch.parent/'tools/verification/system-paths-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 raise SystemExit(r.returncode)
