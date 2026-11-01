"""Compile the real firmware repair state machine against fault-injected storage."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("sketch", type=Path)
parser.add_argument("--cxx", default="g++")
parser.add_argument("--msvc", action="store_true")
parser.add_argument("--volume", action="store_true")
args = parser.parse_args()
here = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="tab5-system-resources-") as temp:
    build = Path(temp)
    for path in here.iterdir():
        if path.suffix in (".h", ".cpp"):
            shutil.copy2(path, build / path.name)
    names = ("Tab5SystemResources.h", "Tab5SystemResources.cpp", "Tab5SystemResourcesData.h", "Tab5Storage.h", "Tab5Storage.cpp")
    if args.volume:
        for path in (here / "volume").iterdir():
            shutil.copy2(path, build / path.name)
        names = ("Tab5VolumeFS.h", "Tab5VolumeFS.cpp", "Tab5Storage.h", "Tab5Storage.cpp", "Tab5SystemPath.h")
    for name in names:
        shutil.copy2(args.sketch / name, build / name)
    exe = build / ("test.exe" if os.name == "nt" else "test")
    sources = ["Tab5VolumeFS.cpp" if args.volume else "Tab5SystemResources.cpp", "Tab5Storage.cpp", "main.cpp"]
    command = ([args.cxx, "/nologo", "/std:c++17", "/EHsc", "/utf-8", "/I."] + sources + ["/Fe:" + str(exe)]) if args.msvc else ([args.cxx, "-std=c++17", "-pthread", "-I."] + sources + ["-o", str(exe)])
    subprocess.run(command, cwd=build, check=True)
    subprocess.run([str(exe)], cwd=build, check=True)
