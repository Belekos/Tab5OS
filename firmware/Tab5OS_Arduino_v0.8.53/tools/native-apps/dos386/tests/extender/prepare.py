#!/usr/bin/env python3
"""Prepare a reproducible DPMI integration fixture using an official binary.

CWSDSTUB is redistributed unmodified with original COFF code appended, as its
distribution terms permit. Its complete release sources and notices accompany
the fixture. This does not claim to rebuild the historical Borland binary.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile

from program import create_executable, coff_image, STUB_SHA256, SUCCESS

BASE = "https://www.delorie.com/pub/djgpp/current/v2misc/"
RELEASES = {
    "csdpmi7b.zip": "deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c",
    "csdpmi7s.zip": "882d29200a720ef5ad3591afc27f7d66a5fc575dddcd6590acea346813aa6d7f",
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def release(name, cache):
    path = cache / name if cache else None
    if path and path.exists():
        data = path.read_bytes()
    else:
        request = urllib.request.Request(BASE + name, headers={"User-Agent": "Tab5OS-DPMI-test"})
        with urllib.request.urlopen(request, timeout=45) as response:
            data = response.read(1024 * 1024 + 1)
    if len(data) > 1024 * 1024 or digest(data) != RELEASES[name]:
        raise ValueError("CWSDPMI release hash mismatch: " + name)
    return data


def prepare(output, cache=None):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    releases = {name: release(name, Path(cache) if cache else None) for name in RELEASES}
    with zipfile.ZipFile(io.BytesIO(releases["csdpmi7b.zip"])) as archive:
        stub = archive.read("bin/CWSDSTUB.EXE")
        documentation = archive.read("bin/cwsdpmi.doc")
    with zipfile.ZipFile(io.BytesIO(releases["csdpmi7s.zip"])) as archive:
        copying = archive.read("src/cwsdpmi/COPYING")
        copying_cws = archive.read("src/cwsdpmi/COPYING.CWS")
    executable = create_executable(stub)
    if executable[:len(stub)] != stub:
        raise AssertionError("Original extender stub was modified")
    (output / "TESTDPMI.EXE").write_bytes(executable)
    (output / "testdpmi.coff").write_bytes(coff_image())
    (output / "CWSDPMI.DOC").write_bytes(documentation)
    (output / "COPYING").write_bytes(copying)
    (output / "COPYING.CWS").write_bytes(copying_cws)
    (output / "program.py").write_bytes(Path(__file__).with_name("program.py").read_bytes())
    (output / "README.md").write_bytes(Path(__file__).with_name("README.md").read_bytes())
    for name, data in releases.items():
        (output / name).write_bytes(data)
    report = {
        "schema": 1, "fixture": "TESTDPMI.EXE", "expectedMarker": SUCCESS,
        "runtime": "CWSDPMI release 7, unmodified CWSDSTUB.EXE + original 32-bit COFF",
        "officialArchiveBase": BASE, "inputArchives": RELEASES,
        "stubSha256": STUB_SHA256, "stubUnmodified": True,
        "fixtureSha256": digest(executable), "fixtureBytes": len(executable),
        "coffSha256": digest(coff_image()), "sourceArchive": "csdpmi7s.zip",
        "coverage": ["32-bit ring-3 DPMI client startup", "DPMI version query",
                     "256 KiB linear allocation", "LDT descriptor setup and release",
                     "read/write across 64 4-KiB pages", "page-crossing dword access",
                     "last allocation dword", "DPMI allocation release", "DOS process exit"],
        "executionTested": False,
        "limits": ["This preparation report does not execute the guest",
                   "The historical CWSDPMI binary is pinned, not rebuilt here",
                   "A passing probe does not establish arbitrary DPMI or DOS-extender compatibility"],
    }
    (output / "extender-provenance.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache", type=Path, help="Optional folder containing the two pinned release ZIPs")
    args = parser.parse_args()
    print(json.dumps(prepare(args.output, args.cache), indent=2))


if __name__ == "__main__":
    main()
