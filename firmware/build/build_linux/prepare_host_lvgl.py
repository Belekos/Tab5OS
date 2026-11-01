"""Build the pinned LVGL library for sanitized host UI tests, outside the release."""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, subprocess, sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl', required=True, type=Path)
    parser.add_argument('--sketch', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    lvgl, sketch, work = args.lvgl.resolve(), args.sketch.resolve(), args.build.resolve()
    if 'version=9.6.0' not in (lvgl/'library.properties').read_text():
        raise RuntimeError('Expected pinned LVGL 9.6.0')
    work.mkdir(parents=True, exist_ok=True)
    config = (sketch/'lv_conf.h').read_text().replace('#define LV_USE_STDLIB_MALLOC LV_STDLIB_CUSTOM', '#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB')
    (work/'lv_conf.h').write_text(config)
    flags = ['-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Wno-deprecated-declarations', '-Wno-cpp', '-DLV_CONF_PATH="'+str(work/'lv_conf.h')+'"']
    flags += ['-I'+str(p) for p in [lvgl, lvgl/'include', sketch]]
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent/'build_clang'))
    from prepare_lvgl import patch_shaper
    original = (lvgl/'src/misc/lv_text_ap.c').read_bytes()
    patched, _, _ = patch_shaper(original)
    (work/'lv_text_ap.c').write_bytes(patched)
    objects = work/'objects'
    objects.mkdir(exist_ok=True)
    sources = [p for p in sorted((lvgl/'src').rglob('*')) if p.suffix in ('.c', '.cpp', '.S') and 'thorvg' not in str(p) and 'vg_lite_tvg' not in str(p)]
    def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
    def compile_source(p):
        target = objects/('_'.join(p.relative_to(lvgl).parts)+'.o')
        source = work/'lv_text_ap.c' if p.name == 'lv_text_ap.c' else p
        stamp = target.with_suffix('.sha256')
        signature = hashlib.sha256((sha(source)+'\n'+repr(flags)).encode()).hexdigest()
        if not target.exists() or not stamp.exists() or stamp.read_text() != signature:
            command = ['g++' if p.suffix == '.cpp' else 'gcc', *flags, '-iquote', str(p.parent), '-c', str(source), '-o', str(target)]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(str(p)+'\n'+result.stdout+result.stderr)
            stamp.write_text(signature)
        return target
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        compiled = list(pool.map(compile_source, sources))
    subprocess.run(['ar', 'rcs', str(work/'liblvgl.a'), *map(str, compiled)], check=True)
    report = {'version':'9.6.0', 'sourceCount':len(sources), 'sourceHashes':{p.relative_to(lvgl).as_posix():sha(p) for p in sources}, 'hostConfigSha256':sha(work/'lv_conf.h'), 'patchedShaperSha256':sha(work/'lv_text_ap.c'), 'archiveSha256':sha(work/'liblvgl.a'), 'sanitizers':['address','undefined']}
    (work/'host-lvgl.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'sourceCount':len(sources), 'archive':str(work/'liblvgl.a'), 'sha256':report['archiveSha256']}))


if __name__ == '__main__':
    main()
