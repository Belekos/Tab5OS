#!/usr/bin/env python3
"""Check a Doom ELF header and record its hash and the current source snapshot.

Run this after the compiler succeeds, from the same unchanged checkout used for
the build. The report records structural validation; it does not execute Doom.
Only the Python standard library is required.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def inspect_elf(data):
    """Require the ELF32 little-endian RISC-V shared-object format of Tab5 apps."""
    if len(data) < 52:
        raise ValueError("ELF file is empty or shorter than an ELF32 header")
    if data[:4] != b"\x7fELF":
        raise ValueError("Expected ELF magic")
    if data[4] != 1:
        raise ValueError("Expected ELF32 class")
    if data[5] != 1:
        raise ValueError("Expected little-endian ELF data")
    if data[6] != 1 or struct.unpack_from("<I", data, 20)[0] != 1:
        raise ValueError("Expected ELF version 1")
    elf_type, machine = struct.unpack_from("<HH", data, 16)
    if elf_type != 3:
        raise ValueError("Expected ET_DYN shared object")
    if machine != 243:
        raise ValueError("Expected RISC-V machine")
    if struct.unpack_from("<H", data, 40)[0] != 52:
        raise ValueError("Invalid ELF32 header size")
    return {
        "class": 32,
        "endianness": "little",
        "machine": machine,
        "machineName": "RISC-V",
        "type": elf_type,
        "typeName": "DYN",
    }


def source_hashes():
    """Include Doom, its build drivers, the runtime, and the SDK interface."""
    verifier = Path(__file__).resolve()
    doom = verifier.parent.parent
    native_apps = doom.parent
    sketch = native_apps.parent.parent
    repository = sketch.parent.parent
    sources = {
        path for path in doom.rglob("*")
        if path.is_file() and path.suffix in (".c", ".h")
    }
    sources.update((doom / "build.py", native_apps / "build.py", verifier))
    # Bind the accompanying regression runner when present in the checkout.
    sources.update(verifier.parent.glob("*.py"))
    sources.add(sketch / "Tab5AppApi.h")
    sources.update(native_apps / "common" / name for name in (
        "runtime.c", "runtime.h", "game_runtime.c", "game_runtime.h",
        "math.c", "nanoprintf.h",
    ))
    return {
        path.relative_to(repository).as_posix(): sha256(path.read_bytes())
        for path in sorted(sources, key=lambda item: item.as_posix())
    }


def write_report(output, report):
    """Publish a complete JSON report without leaving a partially written file."""
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n", prefix=".doom-report-",
            suffix=".tmp", dir=output.parent, delete=False,
        ) as stream:
            temporary = Path(stream.name)
            json.dump(report, stream, indent=2, ensure_ascii=True)
            stream.write("\n")
        os.replace(temporary, output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path, help="Compiled Doom.app.elf to verify")
    parser.add_argument("--output", type=Path, required=True, help="JSON report path")
    args = parser.parse_args(argv)
    artifact = args.elf.resolve()
    output = args.output.resolve()
    try:
        if artifact == output:
            raise ValueError("Report output must differ from the ELF input")
        data = artifact.read_bytes()
        elf = inspect_elf(data)
        report = {
            "schemaVersion": 1,
            "passed": True,
            "artifact": {
                "path": artifact.as_posix(),
                "bytes": len(data),
                "sha256": sha256(data),
            },
            "elf": elf,
            "sourceHashes": source_hashes(),
        }
        write_report(output, report)
    except (OSError, ValueError) as error:
        print("Doom build verification failed: " + str(error), file=sys.stderr)
        return 1
    print("Verified Doom ELF: {} bytes, SHA-256 {}".format(len(data), report["artifact"]["sha256"]))
    print("Report: " + str(output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
