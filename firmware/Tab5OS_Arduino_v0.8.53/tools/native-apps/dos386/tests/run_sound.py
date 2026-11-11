"""Exercise production SB16 PCM/DMA and AdLib generation, optionally sanitized."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--msvc', action='store_true')
    p.add_argument('--sanitize', action='store_true')
    a = p.parse_args()
    if a.msvc and a.sanitize:
        p.error('--sanitize requires GCC or Clang')
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    sources = [root / 'tests/test_sound.c'] + [root / 'core' / n for n in ('i8257.c', 'adlib.c', 'fmopl.c')]
    exe = out / ('sound.exe' if a.msvc else 'sound')
    if a.msvc:
        cmd = ['cl', '/nologo', '/O2', '/std:c11', '/DUSE_FMOPL', '/D_CRT_SECURE_NO_WARNINGS', *map(str, sources), '/Fe:' + str(exe)]
    else:
        cmd = ['cc', '-std=gnu11', '-O2', '-g', '-DUSE_FMOPL', *map(str, sources), '-lm', '-o', str(exe)]
        if a.sanitize:
            cmd[1:1] = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    build = subprocess.run(cmd, cwd=out, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / 'build.log').write_text(build.stdout)
    if build.returncode:
        print(build.stdout)
        raise SystemExit(build.returncode)
    env = os.environ.copy()
    # The actual application frees its whole arena; FMOPL retains lookup tables.
    env['ASAN_OPTIONS'] = 'detect_leaks=0'
    run = subprocess.run([str(exe)], cwd=out, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    (out / 'run.log').write_text(run.stdout)
    report = {'passed': run.returncode == 0, 'compiler': cmd[0], 'sanitizers': a.sanitize, 'output': run.stdout,
              'coverage': ['pcm_formats', 'persistent_rate_phase', 'ring_wrap', 'dma_bounds', 'dsp_reset', 'adlib_tone']}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(run.stdout, end='')
    raise SystemExit(run.returncode)


if __name__ == '__main__':
    main()
