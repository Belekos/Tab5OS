#!/usr/bin/env python3
"""Audit pinned source preparation and ROM validation; does not compile ROMs."""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

BUILDER = Path(__file__).resolve().parents[1] / "prepare_bios.py"
spec = importlib.util.spec_from_file_location("bios_builder", BUILDER)
bios = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bios)
ARCHIVE = None


def fake_archive(name, kind=tarfile.REGTYPE):
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode="w") as archive:
        entry = tarfile.TarInfo(name)
        entry.type = kind
        entry.linkname = "target" if kind != tarfile.REGTYPE else ""
        entry.size = 1 if kind == tarfile.REGTYPE else 0
        archive.addfile(entry, io.BytesIO(b"x") if entry.size else None)
    return stream.getvalue()


class ProvenanceTests(unittest.TestCase):
    def test_pinned_inputs(self):
        inputs = bios.read_inputs()
        self.assertEqual(len(inputs), 3)
        self.assertIn(b"+    set_equipment_flags(0x02, 0x02);", inputs["tab5-x87.patch"])

    def test_extract_rejects_escape_and_links(self):
        with tempfile.TemporaryDirectory(prefix="tab5-bios-test-") as temporary:
            for name, kind in (("../outside", tarfile.REGTYPE), ("/absolute", tarfile.REGTYPE),
                               ("..\\outside", tarfile.REGTYPE), (".git/config", tarfile.REGTYPE),
                               ("link", tarfile.SYMTYPE), ("hardlink", tarfile.LNKTYPE)):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    bios.unpack(fake_archive(name, kind), Path(temporary))

    def test_extract_checks_expected_prefix(self):
        with tempfile.TemporaryDirectory(prefix="tab5-bios-test-") as temporary:
            with self.assertRaises(ValueError):
                bios.unpack(fake_archive("wrong/file"), Path(temporary), "expected")

    def test_archive_hash_mismatch(self):
        with tempfile.TemporaryDirectory(prefix="tab5-bios-test-") as temporary:
            archive = Path(temporary) / "bad.tar"
            archive.write_bytes(fake_archive("file"))
            with self.assertRaises(ValueError):
                bios.acquire(None, archive)

    def test_rom_validation(self):
        system = bytearray(128 * 1024)
        system[:7] = b"SeaBIOS"
        system[-16] = 0xea
        video = bytearray(512)
        video[:3] = b"\x55\xaa\x01"
        video[-1] = -sum(video) & 255
        bios.verify_roms(system, video)
        for bad_system, bad_video in ((system[:-1], video), (bytes(len(system)), video),
                                     (system, video[:-1]), (system, b"bad" + video[3:])):
            with self.subTest(bios_bytes=len(bad_system), vga_bytes=len(bad_video)), self.assertRaises(ValueError):
                bios.verify_roms(bad_system, bad_video)
        video[10] ^= 1
        with self.assertRaises(ValueError):
            bios.verify_roms(system, video)

    def test_source_bundle_reproducibility(self):
        with tempfile.TemporaryDirectory(prefix="tab5-bios-test-") as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "script.sh").write_bytes(b"#!/bin/sh\ntrue\n")
            (source / "file").write_bytes(b"content\n")
            modes = {"script.sh": 0o755, "file": 0o644}
            one, two = root / "one.tar.gz", root / "two.tar.gz"
            bios.source_bundle(source, one, modes)
            bios.source_bundle(source, two, modes)
            self.assertEqual(one.read_bytes(), two.read_bytes())
            with tarfile.open(one) as archive:
                self.assertEqual(archive.getmember("seabios-tab5os/script.sh").mode, 0o755)

    def test_actual_source_preparation(self):
        if ARCHIVE is None:
            self.skipTest("Use --archive for full pinned-source export test")
        with tempfile.TemporaryDirectory(prefix="tab5-bios-test-") as temporary:
            root = Path(temporary)
            bundles = []
            for suffix in ("first", "second"):
                output = root / suffix
                run = subprocess.run([sys.executable, "-B", str(BUILDER), "--archive", str(ARCHIVE),
                                      "--prepare-only", "--output", str(output)], capture_output=True, text=True)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                report = json.loads((output / "bios-provenance.json").read_text())
                self.assertFalse(report["compiled"])
                self.assertFalse(report["reproducible"])
                self.assertFalse((output / "bios.bin").exists())
                self.assertFalse((output / "vgabios.bin").exists())
                bundle = output / "seabios-source.tar.gz"
                bundles.append(bundle.read_bytes())
                self.assertEqual(report["sourceBundleSha256"], hashlib.sha256(bundles[-1]).hexdigest())
                with tarfile.open(bundle) as archive:
                    def contents(path):
                        return archive.extractfile("seabios-tab5os/" + path).read()
                    source_manifest = json.loads(contents("tab5os-build/source-manifest.json"))
                    for name, digest in source_manifest["sourceFiles"].items():
                        self.assertEqual(hashlib.sha256(contents(name)).hexdigest(), digest, name)
                    self.assertIn(b"    set_equipment_flags(0x02, 0x02);", contents("src/misc.c"))
                    self.assertNotIn(b"//    set_equipment_flags(0x02, 0x02);", contents("src/misc.c"))
                    self.assertIn(b"use_emulink", contents("src/hw/floppy.c"))
                    self.assertTrue(contents("COPYING.LESSER"))
                    self.assertEqual(archive.getmember("seabios-tab5os/scripts/test-build.sh").mode, 0o755)
            self.assertEqual(bundles[0], bundles[1])


def main():
    global ARCHIVE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    ARCHIVE = args.archive.resolve() if args.archive else None
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ProvenanceTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {"component": "dos386-bios-provenance", "passed": result.wasSuccessful(),
              "testsRun": result.testsRun, "skipped": len(result.skipped),
              "pinnedSourcePreparationTested": ARCHIVE is not None,
              "romCompilationTested": False, "physicalDeviceTested": False}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(report, indent=2))
    if not result.wasSuccessful():
        raise SystemExit(1)


if __name__ == "__main__":
    main()
