#!/usr/bin/env python3
"""Build the pinned Tiny386-compatible BIOS pair, with corresponding sources.

Requires a Linux GNU build environment. --prepare-only exports and audits the
patched sources without claiming that a ROM was compiled. --archive accepts a
previously downloaded, hash-verified upstream archive; --source accepts a clean
Git checkout at the exact pin. Neither input is modified.
"""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

HERE = Path(__file__).resolve().parent
ASSETS = HERE / "bios"
PIN = "c2a33ad9ad1452e23b41c4ac44a3bc6be8ebc4cf"
TINY_PIN = "a080c5d347f432481710902423c93992edde52d7"
ARCHIVE_SHA256 = "8b286a79fb85cd95437923f98c7f833edb73271ffc0440f6d532a48165294604"
URL = f"https://codeload.github.com/coreboot/seabios/tar.gz/{PIN}"
EPOCH = 1783541896
VERSION = "1.17.0-c2a33ad9"
EXTRA = "-tab5os-tiny386-a080c5d"
INPUT_HASHES = {
    "seabios.config": "539b50349b82d5359b1e796c07824149fdfb6a467e92ab34c83d6ed699bd7f1b",
    "tiny386.patch": "6121475eebee1550a7e04311368d3b981bd9e783c4710a6798e431662593cccd",
    "tab5-x87.patch": "61586a7fd762ba12f55a42511a82a31a9193ad29f3a3593b3ac76ab130b0427a",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def normalized(path):
    return path.read_bytes().replace(b"\r\n", b"\n")


def read_inputs():
    result = {}
    for name, expected in INPUT_HASHES.items():
        data = normalized(ASSETS / name)
        if sha(data) != expected:
            raise ValueError("BIOS input hash mismatch: " + name)
        result[name] = data
    return result


def unpack(data, target, prefix=""):
    """Extract only bounded regular files/directories inside a fresh directory."""
    target = target.resolve()
    modes = {}
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:*") as archive:
        members = archive.getmembers()
        if len(members) > 10000 or sum(m.size for m in members) > 64 * 1024 * 1024:
            raise ValueError("SeaBIOS source archive exceeds budget")
        for member in members:
            name = PurePosixPath(member.name)
            if name.is_absolute() or ".." in name.parts or "\\" in member.name:
                raise ValueError("Unsafe source archive path")
            if prefix:
                if not name.parts or name.parts[0] != prefix:
                    raise ValueError("Unexpected source archive root")
                name = PurePosixPath(*name.parts[1:])
            if not name.parts:
                continue
            if ".git" in name.parts or not (member.isdir() or member.isfile()):
                raise ValueError("Unsupported source archive entry")
            path = (target / str(name)).resolve()
            if not path.is_relative_to(target):
                raise ValueError("Source archive escapes output directory")
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                if member.size > 8 * 1024 * 1024:
                    raise ValueError("Oversized source file")
                path.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as stream:
                    path.write_bytes(stream.read())
                mode = 0o755 if member.mode & 0o111 else 0o644
                path.chmod(mode)
                modes[path.relative_to(target).as_posix()] = mode
    return modes


def acquire(source, archive):
    if source:
        revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
        clean = subprocess.check_output(["git", "-C", str(source), "status", "--porcelain", "--untracked-files=no"], text=True)
        if revision != PIN or clean.strip():
            raise ValueError("--source must be a clean SeaBIOS checkout at " + PIN)
        return subprocess.check_output(["git", "-C", str(source), "archive", "--format=tar", "HEAD"]), ""
    if archive:
        data = archive.read_bytes()
    else:
        request = urllib.request.Request(URL, headers={"User-Agent": "Tab5OS-BIOS-builder"})
        with urllib.request.urlopen(request, timeout=90) as response:
            data = response.read(8 * 1024 * 1024 + 1)
    if len(data) > 8 * 1024 * 1024 or sha(data) != ARCHIVE_SHA256:
        raise ValueError("SeaBIOS upstream archive hash mismatch")
    return data, "seabios-" + PIN


def command(args, cwd, log, env=None):
    log.write(("$ " + " ".join(map(str, args)) + "\n").encode())
    log.flush()
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError("Command failed; see bios-build.log: " + " ".join(map(str, args)))


def prepare(source, inputs, log):
    package = source / "tab5os-build"
    package.mkdir()
    for name, data in inputs.items():
        (package / name).write_bytes(data)
    for name in ("tiny386.patch", "tab5-x87.patch"):
        patch = package / name
        command(["git", "apply", "--check", str(patch)], source, log)
        command(["git", "apply", str(patch)], source, log)
    (source / ".config").write_bytes(inputs["seabios.config"])
    (source / ".version").write_text(VERSION + "\n", encoding="ascii", newline="\n")
    (package / "NOTICE.txt").write_bytes(normalized(ASSETS / "NOTICE.txt"))
    (package / "BUILDING.txt").write_text(
        "This archive already contains the applied Tiny386 and Tab5OS patches.\n"
        "Do not apply them a second time. Build from this directory on Linux:\n\n"
        "  make olddefconfig PYTHON=python3\n"
        f"  make -j2 PYTHON=python3 EXTRAVERSION={EXTRA}\n\n"
        "Outputs: out/bios.bin and out/vgabios.bin.\n"
        "Keep .version and supply EXTRAVERSION to avoid timestamps/hostnames.\n"
        "Use the compiler/binutils versions recorded in bios-provenance.json\n"
        "to reproduce the delivered bytes. The complete Tab5OS builder is\n"
        "included here for review; its inputs are in this same directory.\n",
        encoding="ascii", newline="\n")
    (package / "prepare_bios.py").write_bytes(normalized(Path(__file__)))
    manifest = {
        "upstream": "https://github.com/coreboot/seabios", "commit": PIN,
        "upstreamArchiveSha256": ARCHIVE_SHA256, "tiny386Commit": TINY_PIN,
        "patchesAndConfig": {name: sha(data) for name, data in inputs.items()},
        "version": VERSION + EXTRA, "x87Advertised": True,
        "sourceFiles": {p.relative_to(source).as_posix(): sha(p.read_bytes())
                        for p in sorted(source.rglob("*")) if p.is_file()},
    }
    (package / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return manifest


def source_bundle(source, destination, modes):
    with destination.open("wb") as raw:
        with gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for path in sorted(source.rglob("*")):
                    if not path.is_file():
                        continue
                    info = archive.gettarinfo(str(path), "seabios-tab5os/" + path.relative_to(source).as_posix())
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mtime = EPOCH
                    info.mode = modes.get(path.relative_to(source).as_posix(), 0o644)
                    with path.open("rb") as stream:
                        archive.addfile(info, stream)


def verify_roms(bios, vga):
    if len(bios) != 128 * 1024 or b"SeaBIOS" not in bios:
        raise ValueError("Invalid 128 KiB SeaBIOS image")
    if bios[-16:] in (bytes(16), b"\xff" * 16):
        raise ValueError("Missing BIOS reset entry")
    if len(vga) < 512 or len(vga) > 65536 or len(vga) % 512:
        raise ValueError("Invalid VGA ROM size")
    if vga[:2] != b"\x55\xaa" or vga[2] * 512 != len(vga) or sum(vga) & 255:
        raise ValueError("Invalid VGA ROM header/checksum")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--source", type=Path)
    group.add_argument("--archive", type=Path)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 32:
        parser.error("--jobs must be between 1 and 32")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    for name in ("bios.bin", "vgabios.bin", "seabios-source.tar.gz", "bios-provenance.json"):
        if (out / name).exists():
            raise ValueError("Use a new BIOS output directory; already exists: " + name)
    inputs = read_inputs()
    data, prefix = acquire(args.source.resolve() if args.source else None,
                           args.archive.resolve() if args.archive else None)
    with (out / "bios-build.log").open("wb") as log:
        with tempfile.TemporaryDirectory(prefix="seabios-build-", dir=out) as temporary:
            stage = Path(temporary).resolve()
            if not stage.is_relative_to(out):
                raise ValueError("Invalid build temporary directory")
            source = stage / "source"
            source.mkdir()
            modes = unpack(data, source, prefix)
            manifest = prepare(source, inputs, log)
            bundle = out / "seabios-source.tar.gz"
            source_bundle(source, bundle, modes)
            for name in ("COPYING", "COPYING.LESSER"):
                shutil.copyfile(source / name, out / name)
            (out / "NOTICE.txt").write_bytes(normalized(ASSETS / "NOTICE.txt"))
            report = {"schema": 1, "sourceCommit": PIN, "tiny386Commit": TINY_PIN,
                      "upstreamArchiveSha256": ARCHIVE_SHA256,
                      "inputHashes": manifest["patchesAndConfig"],
                      "sourceBundleSha256": sha(bundle.read_bytes()),
                      "sourceBundle": bundle.name, "x87Advertised": True,
                      "version": VERSION + EXTRA, "compiled": False,
                      "reproducible": False, "physicalDeviceTested": False}
            if not args.prepare_only:
                env = dict(os.environ, LC_ALL="C", TZ="UTC", SOURCE_DATE_EPOCH=str(EPOCH))
                make = ["make", "PYTHON=python3", "EXTRAVERSION=" + EXTRA]
                command(make + ["olddefconfig"], source, log, env)
                outputs = []
                headers = []
                for directory in ("out-first", "out-second"):
                    command(make + ["OUT=" + directory + "/", "-j" + str(args.jobs)], source, log, env)
                    build = source / directory
                    binaries = tuple((build / name).read_bytes() for name in ("bios.bin", "vgabios.bin"))
                    verify_roms(*binaries)
                    for name in ("autoversion.h", "autovgaversion.h"):
                        text = (build / name).read_text()
                        expected = '#define BUILD_VERSION "' + VERSION + EXTRA + '"'
                        if expected not in text:
                            raise ValueError("Non-deterministic BIOS version header: " + name)
                        headers.append(text)
                    outputs.append(binaries)
                if outputs[0] != outputs[1]:
                    raise ValueError("Independent BIOS rebuild differs")
                report.update(compiled=True, reproducible=True, versionHeaders=headers[:2])
                report["roms"] = {}
                for name, binary in zip(("bios.bin", "vgabios.bin"), outputs[0]):
                    (out / name).write_bytes(binary)
                    report["roms"][name] = {"bytes": len(binary), "sha256": sha(binary)}
            (out / "bios-provenance.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
            print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
