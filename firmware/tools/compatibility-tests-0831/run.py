"""Host regression checks; does not compile the ESP32-P4 firmware or exercise hardware."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile

release = Path(__file__).resolve().parents[2]
tests = Path(__file__).resolve().parent
sketches = list(release.glob("Tab5OS_Arduino*/Tab5Audio.cpp"))
preferred = release / "Tab5OS_Arduino_v0.8.31_Ecriture/Tab5Audio.cpp"
if preferred in sketches:
    sketch = preferred.parent
elif len(sketches) == 1:
    sketch = sketches[0].parent
else:
    raise SystemExit("Expected exactly one Arduino sketch")


def extract_method(source, name):
    """Keep exact bytes after UTF-8 decode; account for strings/comments/braces."""
    start = source.index("bool Tab5AudioEngine::" + name + "(")
    brace = source.index("{", start)
    depth, i = 0, brace
    state = "code"
    while i < len(source):
        c = source[i]
        if state == "line":
            if c in "\r\n":
                state = "code"
        elif state == "block":
            if source[i:i+2] == "*/":
                state = "code"
                i += 1
        elif state in ('"', "'"):
            if c == "\\":
                i += 1
            elif c == state:
                state = "code"
        elif source[i:i+2] == "//":
            state = "line"
            i += 1
        elif source[i:i+2] == "/*":
            state = "block"
            i += 1
        elif c in ('"', "'"):
            state = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return source[start:i+1]
        i += 1
    raise ValueError("Unterminated method: " + name)


paths = [sketch / n for n in ("Tab5Compatibility.cpp", "Tab5Compatibility.h",
                             "Tab5CompatibilityPolicy.h", "Tab5Audio.cpp", "Tab5Audio.h")]
paths += sorted(p for p in tests.rglob("*") if p.is_file() and "__pycache__" not in p.parts)


def hashes():
    return {p.relative_to(release).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


before = hashes()
source = (sketch / "Tab5Audio.cpp").read_bytes().decode("utf-8")
methods = {name: extract_method(source, name) for name in (
    "execStartWifiScan", "execConnectWifi", "execSetWifiEnabled")}
compiler = os.environ.get("CXX", "g++")
if not shutil.which(compiler):
    raise SystemExit("A host C++ compiler is required: " + compiler)
results = []
with tempfile.TemporaryDirectory(prefix="tab5-compatibility-0831-") as build_dir:
    build = Path(build_dir)
    (build / "audio_methods.inc").write_bytes("\n\n".join(methods.values()).encode("utf-8"))
    for harness in ("adapter", "audio"):
        binary = build / harness
        command = [compiler, "-std=c++17", "-O1", "-g", "-pthread", "-Wall", "-Wextra",
                   "-Werror", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                   "-fno-pie", "-no-pie", "-I" + str(tests), "-I" + str(sketch), "-I" + str(build),
                   str(tests / (harness + "_test.cpp")), str(sketch / "Tab5Compatibility.cpp"),
                   "-o", str(binary)]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        entry = {"harness": harness, "compileExitCode": compiled.returncode,
                 "compileOutput": compiled.stdout + compiled.stderr, "runs": []}
        if compiled.returncode == 0:
            # Verify the final linked image has neither a version-RPC definition
            # nor an unresolved reference (also enforced by normal static link).
            symbols = subprocess.run(["nm", "-C", str(binary)], capture_output=True, text=True, timeout=10)
            entry["versionRpcAbsentFromSymbols"] = symbols.returncode == 0 and \
                "esp_hosted_get_coprocessor_fwversion" not in symbols.stdout
            for args in ([[]] if harness == "adapter" else [["0"], ["1"], ["2"]]):
                run = subprocess.run([str(binary)] + args, capture_output=True, text=True,
                                     timeout=30, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0:abort_on_error=1"})
                output = run.stdout + run.stderr
                match = re.search(r"PASS (\d+) (?:adapter|audio) checks", output)
                entry["runs"].append({"args": args, "exitCode": run.returncode, "output": output,
                                      "checks": int(match[1]) if match else 0,
                                      "passed": run.returncode == 0 and bool(match)})
                print(output, end="")
        else:
            print(entry["compileOutput"])
        results.append(entry)
after = hashes()
passed = before == after and all(
    r["compileExitCode"] == 0 and r.get("versionRpcAbsentFromSymbols") and r["runs"] and
    all(run["passed"] for run in r["runs"]) for r in results)
report = {
    "version": "0.8.31", "checkedAt": datetime.datetime.now(datetime.timezone.utc).isoformat(),
    "method": "Linux host C++17 + AddressSanitizer/UndefinedBehaviorSanitizer. Exact production compatibility adapter and policy compiled; exact UTF-8 method extracts of execStartWifiScan, execConnectWifi, execSetWifiEnabled compiled with lightweight Wi-Fi/network/state guards. No firmware-version RPC definition is supplied, and linked symbols are checked. Adapter SDK boundaries are mocked.",
    "passed": passed, "physicalDeviceTested": False, "targetFirmwareCompiled": False,
    "limitations": ["No ESP32-P4 SDK/toolchain build was performed.",
                    "LeakSanitizer is disabled because this execution environment cannot expose /proc task information; AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.",
                    "Host Wi-Fi, M5 I2C, ESP-Hosted events and network arbitration are stubs; actual radio/protocol compatibility and display timings require hardware validation.",
                    "Audio tests compile the three extracted production methods, not the entire Tab5Audio.cpp or firmware."],
    "sourceStableDuringTests": before == after,
    "checks": sum(run["checks"] for r in results for run in r["runs"]),
    "results": results, "sourceHashes": before,
    "extractedMethodHashes": {name: hashlib.sha256(code.encode("utf-8")).hexdigest()
                              for name, code in methods.items()},
}
(release / "tools/verification/compatibility-0831.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("PASS" if passed else "FAIL", "host compatibility regression report")
raise SystemExit(0 if passed else 1)
