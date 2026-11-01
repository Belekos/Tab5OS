#!/usr/bin/env python3
"""Run actual copy worker against temporary files and injected POSIX I/O faults."""
from pathlib import Path
import argparse, os, shutil, subprocess, tempfile
p = argparse.ArgumentParser()
p.add_argument('--cxx', default='g++')
a = p.parse_args()
here = Path(__file__).resolve().parent
source = here.parent.parent
with tempfile.TemporaryDirectory(prefix='tab5-file-operation-') as temp:
    build = Path(temp)
    volume = build / 'volume'
    volume.mkdir()
    for item in here.iterdir():
        if item.suffix == '.h' or item.name == 'main.cpp':
            shutil.copy2(item, build / item.name)
    for name in ('Tab5FileOperation.h', 'Tab5FilePolicy.h'):
        shutil.copy2(source / name, build / name)
    policy = build / 'Tab5FilePolicy.h'
    policy.write_text(policy.read_text(encoding='utf-8').replace('inline bool showHidden', 'static bool showHidden'), encoding='utf-8')
    # Only the mount and OS call hooks differ; worker logic remains unchanged.
    code = (source / 'Tab5FileOperation.cpp').read_text(encoding='utf-8')
    code = code.replace('#include <sys/stat.h>', '#include <sys/stat.h>\n#include "io_shim.h"')
    code = code.replace('"/sd%s%s%s"', '"' + volume.as_posix() + '%s%s%s"')
    # The temporary mount prefix is longer than firmware's /sd. Expand only
    # physical scratch path arrays/capacity; logical 255-character limits stay.
    code = code.replace('PathCapacity + 4', 'PathCapacity + 1024').replace('PathCapacity + 3', 'PathCapacity + 1023')
    (build / 'Tab5FileOperation.cpp').write_text(code, encoding='utf-8')
    exe = build / ('copy-test.exe' if os.name == 'nt' else 'copy-test')
    subprocess.run([a.cxx, '-std=c++1z', '-Wall', '-Wextra', '-I', str(build),
                    str(build / 'Tab5FileOperation.cpp'), str(build / 'main.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(volume)], check=True)
