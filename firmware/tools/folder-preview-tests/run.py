"""Compile the unmodified production header beside narrow FS test doubles."""
from pathlib import Path
import hashlib,json
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parent
release = root.parent.parent
sketch = next(release.glob('Tab5OS_Arduino_v*_Ecriture'))
source = sketch / 'Tab5FolderPreview.h'
build = Path(tempfile.mkdtemp(prefix='tab5-folder-preview-'))
for name in ('Tab5VolumeFS.h', 'Tab5HiddenFiles.h', 'test_folder_preview.cpp'):
    shutil.copyfile(root / name, build / name)
shutil.copyfile(source, build / source.name)
for name in ('Tab5DirectoryModel.h','Tab5FileSort.h'):shutil.copyfile(sketch/name,build/name)
signature = hashlib.sha256(source.read_bytes()).hexdigest()
assert hashlib.sha256((build / source.name).read_bytes()).hexdigest() == signature
vcvars = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat')
commands = build / 'run.cmd'
commands.write_text('@echo off\n'
    f'call "{vcvars}" >nul\n'
    f'cl /nologo /utf-8 /std:c++17 /EHsc /W4 /fsanitize=address "{build / "test_folder_preview.cpp"}" /Fe:"{build / "test.exe"}" /Fo:"{build / "test.obj"}"\n'
    'if errorlevel 1 exit /b %errorlevel%\n'
    f'"{build / "test.exe"}"\n', encoding='utf-8')
result = subprocess.run(['cmd', '/d', '/c', str(commands)], capture_output=True)
output = result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')
report = f'Production header SHA-256: {signature}\n{output}\nexit_code={result.returncode}\n'
(root / 'results.txt').write_text(report, encoding='utf-8')
evidence={'version':'0.8.24','passed':result.returncode==0,'physicalDeviceTested':False,'exitCode':result.returncode,
          'sanitizer':'AddressSanitizer','output':output,
          'sources':{name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in ('Tab5FolderPreview.h','Tab5DirectoryModel.h','Tab5FileSort.h')},
          'tests':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in ('run.py','test_folder_preview.cpp','Tab5VolumeFS.h','Tab5HiddenFiles.h')}}
(release/'tools/verification/folder-preview-0824.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(report)
raise SystemExit(result.returncode)
