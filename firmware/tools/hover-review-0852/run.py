"""Build an independent integration harness against actual source snapshots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
for name in ("sketch", "lvgl", "lvgl-build", "build"):
    parser.add_argument("--" + name, type=Path, required=True)
parser.add_argument("--baseline", action="store_true")
args = parser.parse_args()
here = Path(__file__).resolve().parent
args.build.mkdir(parents=True, exist_ok=True)
sources = ["Tab5Mouse.cpp", "Tab5Focus.cpp", "Tab5TextInput.cpp", "Tab5UiStyle.cpp", "WriterDocument.cpp"]
headers = ["Tab5Mouse.h", "Tab5Focus.h", "Tab5UiStyle.h", "Tab5TextInput.h", "WriterKeys.h",
           "WriterUsb.h", "WriterMouse.h", "WriterDocument.h", "Tab5HomeUi.h"]
files = [args.sketch / name for name in sources + headers]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


source_hashes = {path.name: sha(path) for path in files}
home_source = (args.sketch / "Tab5HomeUi.h").read_text()
parts = []
for signature in ("static const char* homeSymbol(", "static void homePassive(",
                  "static lv_obj_t* homeLabel(", "static void homeApplicationCard("):
    start = home_source.index(signature)
    brace = home_source.index("{", start)
    level = 1
    end = brace + 1
    while level:
        if home_source[end] == "{": level += 1
        elif home_source[end] == "}": level -= 1
        end += 1
    parts.append(home_source[start:end])
(args.build / "home_card.inc").write_text("\n".join(parts) + "\n")
(args.build / "result.json").unlink(missing_ok=True)
flags = ["-std=c++17", "-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
         "-no-pie", "-Wno-deprecated-declarations", "-Wno-cpp",
         '-DLV_CONF_PATH="' + str(args.lvgl_build.resolve() / "lv_conf.h") + '"',
         "-I" + str(args.sketch.resolve()), "-I" + str(args.lvgl.resolve()),
         "-I" + str(args.build.resolve())]
if not args.baseline:
    flags.append("-DTAB5_HOVER_FIXED")
command = ["g++", *flags, str(here / "common.cpp"),
           *[str(args.sketch / name) for name in sources if name != "Tab5Mouse.cpp"],
           str(args.lvgl_build / "liblvgl.a"), "-o", str(args.build / "test")]
compiled = subprocess.run(command, capture_output=True, text=True, timeout=180)
(args.build / "compile.log").write_text(compiled.stdout + compiled.stderr)
print(compiled.stdout + compiled.stderr)
compiled.check_returncode()
run = subprocess.run([str(args.build / "test"), *(["baseline"] if args.baseline else [])],
                     capture_output=True, text=True, timeout=30,
                     env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
                              UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"))
(args.build / "run.log").write_text(run.stdout + run.stderr)
print(run.stdout + run.stderr)
run.check_returncode()
if source_hashes != {path.name: sha(path) for path in files}:
    raise RuntimeError("Source changed during compilation/test; rerun against a stable snapshot")
report = {
    "sourceHashes": source_hashes,
    "result": json.loads(run.stdout),
    "compileExitCode": compiled.returncode,
    "runExitCode": run.returncode,
    "compileCommand": command,
    "testHashes": {path.name: sha(path) for path in (here / "common.cpp", here / "run.py")},
    "homeConstructorExtractSha256": sha(args.build / "home_card.inc"),
    "lvglSha256": sha(args.lvgl_build / "liblvgl.a"),
    "lvglConfigSha256": sha(args.lvgl_build / "lv_conf.h"),
    "sanitizers": {"address": True, "undefined": True, "leakDetection": False},
}
(args.build / "result.json").write_text(json.dumps(report, indent=2) + "\n")
