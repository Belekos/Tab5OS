#!/usr/bin/env python3
"""Independent scalar-oracle check of the production framebuffer transfer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--out", type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
sketch = root / "Tab5OS_Arduino_v0.8.47_Ecriture"
fixture = Path(__file__).with_suffix(".cpp")
with tempfile.TemporaryDirectory(prefix="tab5-blit-review-") as work:
    binary = str(Path(work) / "blit-review")
    flags = ["-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
             "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
    command = ["g++", *flags, "-I", str(sketch), str(fixture), "-o", binary]
    subprocess.run(command, check=True)
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1")
    run = subprocess.run([binary], check=True, text=True, capture_output=True, env=env)
    report = json.loads(run.stdout)
report.update({
    "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
    "flags": flags,
    "sanitizers": ["AddressSanitizer", "UndefinedBehaviorSanitizer"],
    "leakSanitizer": False,
    "sourceSha256": {name: hashlib.sha256((sketch / name).read_bytes()).hexdigest()
                     for name in ["Tab5DisplayBlit.h", "Tab5DisplayCacheRanges.h"]},
    "fixtureSha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
    "scope": "Host scalar-oracle and rejection tests; no DMA, cache or physical display claim"
})
text = json.dumps(report, indent=2) + "\n"
if a.out:
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(text)
print(text, end="")
