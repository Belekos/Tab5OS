#!/usr/bin/env python3
"""Run unmodified ESP-IDF/TLSF heap walk, instrumenting only the lock boundary."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--sanitize", action="store_true")
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="tab5-native-heap-") as temporary:
    executable = Path(temporary) / "heap-walk"
    command = ["gcc", "-std=gnu11", "-O2", "-g", "-DMULTI_HEAP_FREERTOS",
               "-I", str(HERE / "stubs"), "-I", str(HERE / "upstream"),
               str(HERE / "experiment.c"), str(HERE / "upstream/multi_heap.c"),
               str(HERE / "upstream/tlsf.c"), "-Wl,--wrap=tlsf_walk_pool",
               "-o", str(executable)]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    result = subprocess.run([str(executable)], text=True, capture_output=True, env=environment)
    if result.returncode:
        print(result.stdout, result.stderr, end="")
        result.check_returncode()
    report = json.loads(result.stdout)
    report["limits"] = [
        "Host pointer size, cache, CPU and elapsed times differ from ESP32-P4.",
        "64 MiB host arena avoids an LP64 metadata layout issue; it is not the 32 MiB Tab5 capacity.",
        "Platform critical-section macros record entry/exit; no real interrupts or DMA are simulated.",
        "Counter reads and allocator walker are real native sources; no fixed heap samples.",
        "Host variant omits LIGHT poisoning; firmware get_info wrapper calls the same impl.",
        "No conclusion about physical display underruns or audible pops is asserted.",
    ]
    report["sanitizers"] = ["AddressSanitizer", "UndefinedBehaviorSanitizer"] if args.sanitize else []
    report["leak_sanitizer"] = "disabled; container /proc restrictions"
    report["source"] = {
        "idf": "https://github.com/espressif/esp-idf/tree/v5.5.4/components/heap",
        "tlsf": "https://github.com/espressif/tlsf/tree/2867f6883a12920b1969ff9624c0ab0e4185c2ce",
        "sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in sorted((HERE / "upstream").iterdir())},
    }
    text = json.dumps(report, indent=2) + "\n"
    (HERE / ("results-sanitized.json" if args.sanitize else "results.json")).write_text(text)
    print(text, end="")
