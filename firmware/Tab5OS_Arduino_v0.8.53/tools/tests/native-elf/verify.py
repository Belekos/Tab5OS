#!/usr/bin/env python3
"""Gate native ELF artifacts with the actual firmware admission parser.

This compiles Tab5ElfValidate.cpp unchanged, then validates every supplied ELF.
It checks structural admission only; it does not load or execute the programs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
SKETCH = HERE.parents[2]
REPO = SKETCH.parent.parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def imports(data):
    """Read imports only after the production validator accepted all tables."""
    shoff = struct.unpack_from("<I", data, 32)[0]
    shnum = struct.unpack_from("<H", data, 48)[0]
    sections = [struct.unpack_from("<10I", data, shoff + 40 * i) for i in range(shnum)]
    result = set()
    for section in sections:
        if section[1] not in (2, 11):  # SHT_SYMTAB or SHT_DYNSYM
            continue
        strings = sections[section[6]]
        for offset in range(section[4], section[4] + section[5], 16):
            name, _, _, _, _, owner = struct.unpack_from("<IIIBBH", data, offset)
            if owner == 0 and name:
                start = strings[4] + name
                end = data.index(b"\0", start, strings[4] + strings[5])
                result.add(data[start:end].decode("ascii"))
    return sorted(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, help="Primary freshly compiled native application")
    parser.add_argument("--expected-import", help="Require this sole import in the primary ELF")
    parser.add_argument("--extra-elf", type=Path, action="append", default=[])
    parser.add_argument("--catalogue", type=Path, help="Also validate all .app.elf files in this directory")
    parser.add_argument("--source-root", type=Path, action="append", default=[],
                        help="Hash native build sources under this repository directory")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.expected_import and not args.elf:
        parser.error("--expected-import requires --elf")
    inputs = ([args.elf.resolve()] if args.elf else []) + [p.resolve() for p in args.extra_elf]
    if args.catalogue:
        catalogue = sorted(args.catalogue.resolve().glob("*.app.elf"))
        if not catalogue:
            parser.error("Catalogue contains no native applications")
        inputs.extend(catalogue)
    inputs = list(dict.fromkeys(inputs))
    if not inputs:
        parser.error("Supply --elf or --catalogue")
    args.output = args.output.resolve()
    if args.output in inputs:
        parser.error("Report output must differ from ELF inputs")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sources = [SKETCH / "Tab5ElfValidate.cpp", SKETCH / "Tab5ElfValidate.h",
               SKETCH / "Tab5AppApi.h", SKETCH / "tools/native-apps/build.py",
               HERE / "validate_host.cpp", Path(__file__).resolve()]
    for source_root in args.source_root:
        root = source_root.resolve()
        if not root.is_relative_to(REPO) or not root.is_dir():
            parser.error("--source-root must name a directory inside this repository")
        sources.extend(p for p in root.rglob("*") if p.is_file() and p.suffix in (".c", ".h", ".py"))
    before = {p.relative_to(REPO).as_posix(): digest(p) for p in sorted(set(sources))}
    if os.name == "nt" and not os.environ.get("VSCMD_VER"):
        vcvars = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat")
        if not vcvars.is_file():
            parser.error("Run from an x64 MSVC developer shell")
        wrapper = args.output.with_suffix(".cmd")
        wrapper.write_text('@echo off\ncall "' + str(vcvars) + '" >nul\n' +
                           subprocess.list2cmdline([sys.executable, "-B", str(Path(__file__).resolve()),
                                                   *sys.argv[1:]]) + "\n", encoding="utf8")
        return subprocess.call(["cmd", "/d", "/c", str(wrapper)])
    with tempfile.TemporaryDirectory(prefix="tab5-native-admission-") as temp:
        work = Path(temp).resolve()
        if not work.is_relative_to(Path(tempfile.gettempdir()).resolve()):
            raise RuntimeError("Temporary build escaped the system temporary directory")
        if os.name == "nt":
            compiler, sanitizers = "cl", []
            executable = work / "validate-host.exe"
            flags = ["/nologo", "/std:c++17", "/EHsc", "/utf-8", "/O2", "/I" + str(SKETCH)]
            command = [compiler, *flags, str(HERE / "validate_host.cpp"),
                       str(SKETCH / "Tab5ElfValidate.cpp"), "/Fe:" + str(executable)]
        else:
            compiler = os.environ.get("CXX", "c++")
            sanitizers = ["AddressSanitizer", "UndefinedBehaviorSanitizer"]
            executable = work / "validate-host"
            flags = ["-std=c++17", "-Wall", "-Wextra", "-g", "-O1", "-fsanitize=address,undefined",
                     "-fno-omit-frame-pointer", "-I" + str(SKETCH)]
            command = [compiler, *flags, str(HERE / "validate_host.cpp"),
                       str(SKETCH / "Tab5ElfValidate.cpp"), "-o", str(executable)]
        built = subprocess.run(command, cwd=work, text=True, encoding="utf8", errors="replace", capture_output=True)
        args.output.with_suffix(".compile.log").write_text(built.stdout + built.stderr, encoding="utf8")
        if built.returncode:
            print(built.stdout + built.stderr)
            built.check_returncode()
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
                   UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        results = []
        for path in inputs:
            data = path.read_bytes()
            # Validate the same immutable bytes that the report hashes and inspects.
            snapshot = work / ("input-" + str(len(results)) + ".elf")
            snapshot.write_bytes(data)
            run = subprocess.run([str(executable), str(snapshot)], cwd=work, env=env,
                                 text=True, encoding="utf8", errors="replace", capture_output=True)
            item = {"artifact": path.relative_to(REPO).as_posix() if path.is_relative_to(REPO) else path.name,
                    "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                    "passed": run.returncode == 0}
            if run.returncode:
                item["error"] = run.stderr.strip() or run.stdout.strip()
            else:
                item["image"] = json.loads(run.stdout)
                item["imports"] = imports(data)
                if args.expected_import and path == args.elf.resolve():
                    item["expectedImport"] = args.expected_import
                    if item["imports"] != [args.expected_import]:
                        item["passed"] = False
                        item["error"] = "Primary application imports do not match the expected ABI"
            item["artifactUnchanged"] = digest(path) == item["sha256"]
            if not item["artifactUnchanged"]:
                item["passed"] = False
                item["error"] = "ELF changed during validation"
            results.append(item)
            print(("PASS " if item["passed"] else "FAIL ") + item["artifact"])
        after = {p.relative_to(REPO).as_posix(): digest(p) for p in sorted(set(sources))}
        unchanged = before == after
        report = {"passed": unchanged and all(item["passed"] for item in results),
                  "component": "Production native ELF admission parser", "compiler": compiler,
                  "flags": flags, "sanitizers": sanitizers, "sourcesUnchanged": unchanged,
                  "sourceHashes": after, "artifacts": results, "physicalDeviceTested": False,
                  "limitations": ["Structural admission and import validation only; programs are not loaded or executed."]}
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        print(args.output)
        return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
