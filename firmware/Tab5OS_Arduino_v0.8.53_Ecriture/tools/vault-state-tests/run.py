#!/usr/bin/env python3
"""Host vault state tests. NVS, crypto, RTOS and Arduino are test doubles."""
from pathlib import Path
import argparse, os, shutil, subprocess, tempfile
p = argparse.ArgumentParser()
p.add_argument('--cxx', default='g++')
args = p.parse_args()
here = Path(__file__).resolve().parent
source = here.parent.parent
with tempfile.TemporaryDirectory(prefix='tab5-vault-') as temp:
    build = Path(temp)
    for item in here.iterdir():
        if item.suffix == '.h' or item.name == 'main.cpp':
            shutil.copy2(item, build / item.name)
    shutil.copytree(here / 'freertos', build / 'freertos')
    for name in ('Tab5KeyStore.cpp', 'Tab5KeyStore.h', 'Tab5VaultCore.h'):
        shutil.copy2(source / name, build / name)
    exe = build / ('vault-test.exe' if os.name == 'nt' else 'vault-test')
    subprocess.run([args.cxx, '-std=c++14', '-Wall', '-Wextra', '-I', str(build),
                    str(build / 'Tab5KeyStore.cpp'), str(build / 'main.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
