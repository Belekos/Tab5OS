"""Compile unmodified native LVGL TLSF and the production elastic adapter; ASan/UBSan."""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, os, subprocess, sys

here = Path(__file__).resolve().parent
release = here.parent.parent
sketch = next(release.glob('Tab5OS_Arduino*_Ecriture'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(command, log):
    result = subprocess.run(list(map(str, command)), capture_output=True, text=True,
                            env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(str(log) + '\n' + result.stdout + result.stderr)
    return result.stdout

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--lvgl', required=True, type=Path, help='Unmodified LVGL 9.6.0 source')
p.add_argument('--build', required=True, type=Path)
p.add_argument('--jobs', type=int, default=6)
a = p.parse_args()
lvgl, build = a.lvgl.resolve(), a.build.resolve()
if 'version=9.6.0' not in (lvgl/'library.properties').read_text():
    raise RuntimeError('Expected LVGL 9.6.0')
build.mkdir(parents=True, exist_ok=True)
config = sketch/'lv_conf.h'
(build/'lv_conf.h').write_bytes(config.read_bytes())
production = [sketch/'Tab5LvglMemory.cpp', sketch/'Tab5LvglMemory.h', config]
source_hashes = {str(f.relative_to(release)): sha(f) for f in production}
flags = ['-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
         '-Wno-deprecated-declarations', '-Wno-cpp',
         '-DLV_CONF_PATH="'+str(build/'lv_conf.h')+'"']
flags += ['-I'+str(f) for f in (lvgl, lvgl/'include', lvgl/'src', sketch)]
sys.path.insert(0, str(release/'tools/build_clang'))
from prepare_lvgl import patch_shaper
shaper, _, _ = patch_shaper((lvgl/'src/misc/lv_text_ap.c').read_bytes())
(build/'lv_text_ap.c').write_bytes(shaper)
objects = build/'objects'
objects.mkdir(exist_ok=True)
sources = [f for f in sorted((lvgl/'src').rglob('*')) if f.suffix in ('.c', '.cpp', '.S')
           and 'thorvg' not in str(f) and 'vg_lite_tvg' not in str(f)]
def compile_source(source):
    obj = objects/('_'.join(source.relative_to(lvgl).parts)+'.o')
    selected = build/'lv_text_ap.c' if source.name == 'lv_text_ap.c' else source
    signature = hashlib.sha256((sha(selected)+repr(flags)+sha(config)).encode()).hexdigest()
    stamp = obj.with_suffix('.sha256')
    if not obj.exists() or not stamp.exists() or stamp.read_text() != signature:
        run(['g++' if source.suffix == '.cpp' else 'gcc', *flags, '-iquote', source.parent,
             '-c', selected, '-o', obj], obj.with_suffix('.log'))
        stamp.write_text(signature)
    return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as executor:
    compiled = list(executor.map(compile_source, sources))
run(['ar', 'rcs', build/'liblvgl.a', *compiled], build/'archive.log')
command = ['g++', '-std=c++17', *flags, '-no-pie', '-I'+str(here/'stubs'),
           here/'test.cpp', sketch/'Tab5LvglMemory.cpp', build/'liblvgl.a',
           '-pthread', '-o', build/'test']
run(command, build/'compile.log')
output = run([build/'test'], build/'test.log')
assert source_hashes == {str(f.relative_to(release)): sha(f) for f in production}
report = {
    'passed': True, 'physicalDeviceTested': False,
    'sourceHashes': source_hashes,
    'testHashes': {str(f.relative_to(release)): sha(f) for f in here.rglob('*') if f.is_file()},
    'nativeTlsfSha256': sha(lvgl/'src/stdlib/builtin/lv_tlsf.c'),
    'lvglVersion': '9.6.0', 'lvglConfigSha256': sha(config),
    'lvglSourceCount': len(sources), 'lvglArchiveSha256': sha(build/'liblvgl.a'),
    'sanitizers': ['address', 'undefined'], 'result': output.strip().splitlines(),
    'boundaries': ['ESP heap capabilities and allocation failures are simulated.',
                   'Task mutex maps to std::mutex; four real host threads execute native TLSF.',
                   'No ESP32 timing, PSRAM bandwidth, DSI or audio result is claimed.',
                   'Valid allocation ownership follows malloc/free rules; invalid/interior/double frees are outside that API contract.',
                   'Host 64-bit TLSF control size differs from ESP32-P4; use target object evidence.']}
(build/'result.json').write_text(json.dumps(report, indent=2)+'\n')
print(output, end='')
print(build/'result.json')
