"""Exercise the actual selection header, independent of widgets and Arduino."""
from pathlib import Path
import hashlib, shutil, subprocess, tempfile
root=Path(__file__).resolve().parent
source=next(root.parent.parent.glob('Tab5OS_Arduino_v*_Ecriture'))/'Tab5FileSelection.h'
build=Path(tempfile.mkdtemp(prefix='tab5-selection-'))
for path in (source,root/'test_file_selection.cpp'):shutil.copyfile(path,build/path.name)
vcvars=Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat')
script=build/'run.cmd'
script.write_text('@echo off\n'+f'call "{vcvars}" >nul\ncl /nologo /std:c++17 /EHsc /W4 "{build / "test_file_selection.cpp"}" /Fe:"{build / "test.exe"}" /Fo:"{build / "test.obj"}"\nif errorlevel 1 exit /b %errorlevel%\n"{build / "test.exe"}"\n','utf-8')
r=subprocess.run(['cmd','/d','/c',str(script)],capture_output=True)
report=f'Production header SHA256: {hashlib.sha256(source.read_bytes()).hexdigest()}\n'+r.stdout.decode('utf-8','replace')+r.stderr.decode('utf-8','replace')+f'\nexit_code={r.returncode}\n'
(root/'results.txt').write_text(report,'utf-8');print(report);raise SystemExit(r.returncode)
