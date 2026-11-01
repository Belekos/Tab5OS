"""Run the focused native tests with MSVC on Windows (no device is emulated)."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
sketch = next(here.parent.parent.glob('Tab5OS_Arduino_v*_Ecriture'))
build = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(tempfile.mkdtemp(prefix='tab5-wiki-'))
build.mkdir(parents=True, exist_ok=True)
vcvars = Path(os.environ.get('ProgramFiles', r'C:\Program Files')) / 'Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
setup_script = build / 'compiler-environment.cmd'
setup_script.write_text('@echo off\ncall "' + str(vcvars) + '" >nul\nif errorlevel 1 exit /b 1\nset\n', encoding='ascii')
setup = subprocess.run(['cmd', '/d', '/c', str(setup_script)], capture_output=True, text=True, check=True)
environment = {key.upper(): value for key, value in os.environ.items()}
for line in setup.stdout.splitlines():
    key, separator, value = line.partition('=')
    if separator and key:
        environment[key.upper()] = value

def run(arguments):
    if arguments[0] == 'cl':
        arguments[0] = Path(environment['VCTOOLSINSTALLDIR']) / 'bin/Hostx64/x64/cl.exe'
    subprocess.run([str(arg) for arg in arguments], cwd=build, env=environment, check=True)

run([sys.executable, here / 'make_fixtures.py', build / 'fixtures'])
sources = [sketch / 'src/xz' / name for name in ('xz_crc32.c', 'xz_crc64.c', 'xz_dec_lzma2.c', 'xz_dec_stream.c')]
sources.append(sketch / 'src/zstd/zstddeclib.c')
for source in sources:
    run(['cl', '/nologo', '/c', '/O2', '/W3', '/TC', source, '/Fo' + str(build / (source.stem + '.obj'))])
objects = [build / (source.stem + '.obj') for source in sources]
run(['cl', '/nologo', '/std:c++17', '/EHsc', '/O2', '/W3', '/I' + str(sketch),
     here / 'wikipedia_qa.cpp', sketch / 'Tab5Zim.cpp', *objects, '/Fe' + str(build / 'wikipedia_qa.exe')])
run([build / 'wikipedia_qa.exe', build / 'fixtures'])
