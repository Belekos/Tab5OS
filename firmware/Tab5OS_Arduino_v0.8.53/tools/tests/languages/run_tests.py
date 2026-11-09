#!/usr/bin/env python3
"""Exercise the production localization loader, parser and catalogue on the host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
SKETCH = HERE.parents[2]
REPO = SKETCH.parent.parent
SOURCES = ["Tab5Localization.cpp", "Tab5Localization.h", "Tab5LanguageReader.h",
           "Tab5LanguageMetadata.h", "Tab5TranslationFormat.h", "Tab5Json.h",
           "Tab5JsonFile.h", "Tab5EnglishFallback.h", "Tab5EnglishBitmap.h"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduinojson", type=Path, required=True,
                        help="Pinned ArduinoJson 7.4.3 library directory")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.arduinojson = args.arduinojson.resolve()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    props = dict(line.split("=", 1) for line in
                 (args.arduinojson / "library.properties").read_text(encoding="utf8").splitlines() if "=" in line)
    if props.get("version") != "7.4.3":
        raise RuntimeError("Expected pinned ArduinoJson 7.4.3")
    if os.name == "nt" and not os.environ.get("VSCMD_VER"):
        command = args.output.with_suffix(".cmd")
        vcvars = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat")
        if not vcvars.is_file():
            raise RuntimeError("Run from an x64 MSVC developer shell")
        command.write_text('@echo off\ncall "' + str(vcvars) + '" >nul\n' +
                           subprocess.list2cmdline([sys.executable, "-B", str(Path(__file__).resolve()),
                                                   "--arduinojson", str(args.arduinojson),
                                                   "--output", str(args.output)]) + "\n", encoding="utf8")
        raise SystemExit(subprocess.call(["cmd", "/d", "/c", str(command)]))
    with tempfile.TemporaryDirectory(prefix="tab5-language-host-") as temp:
        work = Path(temp).resolve()
        if not work.is_relative_to(Path(tempfile.gettempdir()).resolve()):
            raise RuntimeError("Temporary test directory escaped the system temporary root")
        for name in SOURCES:
            shutil.copyfile(SKETCH / name, work / name)
        for name in ("test_languages.cpp", "host_platform.h"):
            shutil.copyfile(HERE / name, work / name)
        for name in ("Arduino.h", "FS.h", "SD.h", "Preferences.h", "esp_heap_caps.h",
                     "Tab5VolumeFS.h", "freertos/FreeRTOS.h", "freertos/semphr.h"):
            p = work / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('#pragma once\n#include "host_platform.h"\n', encoding="ascii")
        # Python's independent JSON parser supplies the exact expected texts.
        # This catches a successful selection that silently falls back to English.
        expected = work / "expected-translations.tsv"
        with expected.open("w", encoding="ascii", newline="\n") as stream:
            for pack_path in sorted((SKETCH / "sdcard/System/languages").glob("*.json")):
                pack = json.loads(pack_path.read_text(encoding="utf8"))
                for source, target in pack["translations"].items():
                    if isinstance(target, str) and target:
                        stream.write("\t".join((pack["id"], source.encode("utf8").hex(),
                                                target.encode("utf8").hex())) + "\n")
        if os.name == "nt":
            executable = work / "language-tests.exe"
            compiler = "cl"
            flags = ["/nologo", "/std:c++17", "/EHsc", "/utf-8", "/O2", "/D_CRT_SECURE_NO_WARNINGS",
                     "/I" + str(work), "/I" + str(args.arduinojson / "src")]
            command = [compiler, *flags, str(work / "test_languages.cpp"), "/Fe:" + str(executable)]
            sanitizers = []
        else:
            executable = work / "language-tests"
            compiler = os.environ.get("CXX", "c++")
            flags = ["-std=c++17", "-Wall", "-Wextra", "-g", "-O1", "-fsanitize=address,undefined",
                     "-fno-omit-frame-pointer", "-I" + str(work), "-I" + str(args.arduinojson / "src")]
            command = [compiler, *flags, str(work / "test_languages.cpp"), "-o", str(executable)]
            sanitizers = ["AddressSanitizer", "UndefinedBehaviorSanitizer"]
        built = subprocess.run(command, cwd=work, text=True, encoding="utf8", errors="replace", capture_output=True)
        args.output.with_suffix(".compile.log").write_text(built.stdout + built.stderr, encoding="utf8")
        if built.returncode:
            print(built.stdout + built.stderr)
            built.check_returncode()
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        executed = subprocess.run([str(executable), str(SKETCH / "sdcard/System/languages"), str(expected)],
                                  cwd=work, env=env, text=True, encoding="utf8", capture_output=True)
        args.output.with_suffix(".run.log").write_text(executed.stdout + executed.stderr, encoding="utf8")
        print(executed.stdout + executed.stderr, end="")
        executed.check_returncode()
        result = json.loads(executed.stdout.strip().splitlines()[-1])
    source_files = [SKETCH / name for name in SOURCES] + list(HERE.glob("*.cpp")) + list(HERE.glob("*.h")) + [Path(__file__).resolve()]
    packs = sorted((SKETCH / "sdcard/System/languages").glob("*.json"))
    result.update({
        "component": "Production language selection, metadata and JSON loader",
        "sanitizers": sanitizers,
        "compiler": compiler,
        "flags": flags,
        "arduinoJsonVersion": props["version"],
        "arduinoJsonPropertiesSha256": hashlib.sha256((args.arduinojson / "library.properties").read_bytes()).hexdigest(),
        "sourceHashes": {p.relative_to(REPO).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files},
        "packHashes": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in packs},
        "limitations": ["Host storage, preferences, PSRAM and synchronization adapters; no physical SD, FreeRTOS scheduling or display test.",
                        "All adapter PSRAM allocations are tracked and must be released; LeakSanitizer disabled for portability."]})
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    print(args.output)


if __name__ == "__main__":
    main()
