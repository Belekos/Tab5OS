"""Run production radio Python and extracted OS controllers with real LVGL/Writer.

Only SDK/VM/window creation boundaries are simulated. No device or Internet use.
"""
from pathlib import Path
import hashlib, json, os, re, subprocess, sys

here = Path(__file__).resolve().parent
release = here.parent.parent
version = re.search(r'v(\d+\.\d+\.\d+)$', release.name).group(1)
sketch = release / ('Tab5OS_Arduino_v' + version + '_Ecriture')
tag = version.replace('.', '')
build = Path('C:/workspace/tab5-launch-radio-' + tag)
build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command = build / 'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"' + sys.executable + '" -X utf8 "' + str(Path(__file__).resolve()) + '"\n', encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd', '/d', '/c', str(command)]))

def block(text, signature):
    begin = text.index(signature)
    brace = text.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        if text[end] == '{': depth += 1
        elif text[end] == '}': depth -= 1
        end += 1
    return text[begin:end]

writing = (sketch / 'Tab5Writing.cpp').read_text(encoding='utf-8')
main = (sketch / ('Tab5OS_Arduino_v' + version + '_Ecriture.ino')).read_text(encoding='utf-8')
tools = (sketch / 'Tab5Tools.cpp').read_text(encoding='utf-8')
source = (here / 'controllers.cpp').read_text(encoding='utf-8')
pieces = {
    'RUN_CODE': block(writing, 'void runCode()'),
    'TEXT_SIZE': block(writing, 'void Tab5WritingApp::setTextSize('),
    'RESUME_OS': block(main, 'static void resumePythonApp() {'),
    'RESUME_TOOLS': block(tools, 'bool Tab5ToolsApp::resumePythonApp(){'),
    'PYTHON_SERVICE': block(tools, 'if(a->tool==Tab5Tool::Python){if(a->pythonWasRunning'),
}
for key, value in pieces.items(): source = source.replace('/* ' + key + ' */', value)
(build / 'controllers.cpp').write_text(source, encoding='utf-8')
lvgl = Path('C:/workspace/tab5-deps-0815/lvgl')
library = Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
flags = ['/nologo', '/utf-8', '/DLV_CONF_SKIP', '/DLV_USE_STDLIB_MALLOC=1', '/DLV_LABEL_TEXT_SELECTION=1', '/DLV_USE_OS=0', '/DLV_FONT_MONTSERRAT_20=1', '/DLV_FONT_MONTSERRAT_24=1', '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(lvgl), '/I' + str(lvgl / 'include'), '/I' + str(sketch)]
command = ['cl', *flags, '/EHsc', '/std:c++17', str(build / 'controllers.cpp'), *[str(sketch / name) for name in ('WriterView.cpp', 'WriterRich.cpp', 'WriterSyntax.cpp', 'WriterDocument.cpp')], str(lvgl / 'src/font/lv_font_montserrat_20.c'), str(lvgl / 'src/font/lv_font_montserrat_24.c'), str(library), '/Fe:' + str(build / 'controllers.exe')]
compiled = subprocess.run(command, cwd=build, capture_output=True)
compile_log = compiled.stdout.decode('utf-8', 'replace') + compiled.stderr.decode('utf-8', 'replace')
(build / 'compile.log').write_text(compile_log, encoding='utf-8')
if compiled.returncode:
    print(compile_log)
    raise SystemExit(compiled.returncode)
native = subprocess.run([str(build / 'controllers.exe')], cwd=build, capture_output=True, encoding='utf-8')
radio = subprocess.run([sys.executable, '-X', 'utf8', str(here / 'radio.py'), str(sketch)], capture_output=True, encoding='utf-8')
print(native.stdout + native.stderr + radio.stdout + radio.stderr)
names = ['Tab5Writing.cpp', 'Tab5Writing.h', 'WriterView.cpp', 'WriterRich.cpp', 'WriterSyntax.cpp', 'WriterDocument.cpp', 'Tab5Tools.cpp', 'Tab5OS_Arduino_v' + version + '_Ecriture.ino', 'sdcard/MicroPython/lib/tab5catalogs.py']
report = {
    'version': version, 'passed': native.returncode == 0 and radio.returncode == 0,
    'method': 'Extracted production Writing run/size, OS and Tools resume/service controllers; actual LVGL9.6, Writer RichView/RichText/Document; VM/window boundaries mocked. Production Python radio module with API/storage/UI stubs. No device test.',
    'native': {'exitCode': native.returncode, 'output': native.stdout + native.stderr},
    'radio': {'exitCode': radio.returncode, 'output': radio.stdout + radio.stderr},
    'sources': {name: hashlib.sha256((sketch / name).read_bytes()).hexdigest() for name in names},
    'tests': {str(path.relative_to(release)): hashlib.sha256(path.read_bytes()).hexdigest() for path in here.glob('*') if path.is_file()},
    'physicalDeviceTested': False,
}
(release / ('tools/verification/launch-radio-' + tag + '.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
