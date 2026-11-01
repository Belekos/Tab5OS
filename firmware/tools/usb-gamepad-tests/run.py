"""Compile and execute the actual gamepad/HID sources with MSVC or a C++ compiler."""
from pathlib import Path
import argparse, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);p.add_argument('--cxx',default='cl');a=p.parse_args()
sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tab5-gamepad-') as name:
 d=Path(name);sources=[str(here/'test_gamepad.cpp'),str(sketch/'WriterGamepad.cpp'),str(sketch/'WriterHid.cpp')]
 if Path(a.cxx).stem.lower() in ('cl','clang-cl'):
  command=[a.cxx,'/nologo','/std:c++17','/EHsc','/W4','/utf-8','/I'+str(sketch),*sources,'/Fe:gamepad.exe']
 else:
  command=[a.cxx,'-std=c++17','-Wall','-Wextra','-I'+str(sketch),*sources,'-o','gamepad.exe']
 subprocess.run(command,cwd=d,check=True)
 subprocess.run([str(d/'gamepad.exe')],cwd=d,check=True)
