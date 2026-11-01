"""Package 0.8.29 only after checking its own build and regression evidence.

Historical 0.8.28 reports and binaries cannot satisfy these checks.
This script does not compile, flash, or publish anything.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import tempfile
import zipfile

VERSION = "0.8.29"
SKETCH = "Tab5OS_Arduino_v0.8.29_Ecriture"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def safe_path(root, relative):
    path = (root / relative).resolve()
    require(path.is_relative_to(root.resolve()), "Path outside release: " + relative)
    return path


def validate(root):
    sketch = root / SKETCH
    proofs = root / "tools/verification"
    require([p.name for p in root.glob("Tab5OS_Arduino*_Ecriture") if p.is_dir()] == [SKETCH],
            "The package must contain exactly the 0.8.29 sketch")
    report = read(proofs / "firmware-verification-0829.json")
    require(report["version"] == VERSION and report["passed"] is True and
            report["buildExitCode"] == 0 and report["sourcesUnchanged"] is True,
            "Missing successful 0.8.29 firmware verification")
    build = read(proofs / "build-0829/build-result.json")
    require(build["exitCode"] == 0 and build["sourcesUnchanged"] is True,
            "Full compilation failed or source files changed")
    manifest = read(proofs / "build-0829/source-manifest-end.json")["files"]
    require(read(proofs / "build-0829/source-manifest-start.json")["files"] == manifest,
            "The sources changed during compilation")
    actual = []
    for path in sorted(sketch.rglob("*")):
        if path.is_file() and path.relative_to(sketch).parts[0] != "sdcard":
            actual.append({"path": path.relative_to(sketch).as_posix(),
                           "bytes": path.stat().st_size, "sha256": digest(path)})
    require(manifest == actual, "The packaged sources differ from the compiled sources")
    evidence = report["evidenceSha256"]
    require(isinstance(evidence, dict) and
            {"build-result.json", "source-manifest-start.json", "source-manifest-end.json",
             "compile-args.json", "build.log"}.issubset(evidence),
            "Incomplete build evidence hashes")
    for name, sha in evidence.items():
        require(digest(safe_path(proofs / "build-0829", name)) == sha,
                "Build evidence changed: " + name)
    for name in ("compatibility-0829.json", "autosave-battery-0829.json"):
        test = read(proofs / name)
        require(test["version"] == VERSION and test["passed"] is True,
                "Regression test did not pass: " + name)
        require(bool(test["sourceHashes"]), "Empty test source evidence: " + name)
        for relative, sha in test["sourceHashes"].items():
            require(digest(safe_path(root, relative)) == sha,
                    "Tested source or fixture changed: " + relative)
    languages = read(proofs / "languages-0829.json")
    require(languages["release"] == VERSION and languages["passed"] is True,
            "Language validation did not pass")
    for language, row in languages["packs"].items():
        require(digest(sketch / "sdcard/System/languages" / (language + ".json")) == row["sha256"],
                "Language changed after validation: " + language)
    resources = read(proofs / "resources-0829.json")
    require(resources["version"] == VERSION and resources["result"] == "PASS",
            "Resource validation did not pass")
    require({SKETCH + "/Tab5SystemResourcesData.h",
             SKETCH + "/sdcard/Factory/Tab5OS-0.8.29/manifest.json"}.issubset(resources["sourceHashes"]),
            "Missing resource fingerprints")
    for relative, sha in resources["sourceHashes"].items():
        require(digest(safe_path(root, relative)) == sha,
                "Resource payload changed after validation: " + relative)
    sd = sketch / "sdcard"
    factory = sd / "Factory/Tab5OS-0.8.29"
    factory_manifest = read(factory / "manifest.json")
    require(factory_manifest["version"] == VERSION, "Wrong Factory version")
    for relative, sha in factory_manifest["files"].items():
        require(digest(safe_path(sd, relative)) == sha and
                digest(safe_path(factory, relative)) == sha,
                "Factory resource mismatch: " + relative)
    artifacts = report["artifacts"]
    if isinstance(artifacts, dict):
        artifacts = [dict(row, suffix=suffix) for suffix, row in artifacts.items()]
    by_suffix = {row["suffix"]: row for row in artifacts}
    for suffix in (".bin", ".elf", ".map"):
        row = by_suffix[suffix]
        require(row["file"] == SKETCH + ".ino" + suffix,
                "Wrong version in firmware artifact: " + row["file"])
        path = (root / "firmware" / ("Tab5OS_v" + VERSION + ".bin") if suffix == ".bin"
                else root / "firmware/debug" / row["file"])
        require(path.stat().st_size == row["bytes"] and digest(path) == row["sha256"],
                "Delivered firmware artifact differs from verified build: " + suffix)
    require({p.name for p in (root / "firmware").glob("*.bin")} == {"Tab5OS_v0.8.29.bin"},
            "Obsolete or unexpected firmware image")
    expected_debug = {SKETCH + ".ino.elf", SKETCH + ".ino.map"}
    require({p.name for p in (root / "firmware/debug").iterdir() if p.is_file()} == expected_debug,
            "Unexpected firmware diagnostic artifact")
    expected_sha = by_suffix[".bin"]["sha256"] + "  Tab5OS_v0.8.29.bin"
    require((root / "firmware/SHA256.txt").read_text().strip() == expected_sha,
            "OTA SHA256 file differs from verified image")
    publication = read(proofs / "publication-0829.json")
    require(publication["version"] == VERSION and publication["passed"] is True,
            "Publication inventory did not pass")
    require(digest(root / "tools/audit_publication.py") == publication["auditScriptSha256"],
            "Publication audit changed")
    inventory = safe_path(root, publication["scope"]["manifest"])
    require(digest(inventory) == publication["scope"]["sha256"], "Publication inventory changed")
    inventory_rows = read(inventory)["files"]
    inventory_paths = set()
    for row in inventory_rows:
        require(digest(safe_path(root, row["path"])) == row["sha256"],
                "File changed after publication inventory: " + row["path"])
        inventory_paths.add(row["path"])
    actual_paths = set()
    for path in root.rglob("*"):
        relative = path.relative_to(root)
        if (not path.is_file() or any(p in {".git", "__pycache__", "build-environment"} for p in relative.parts)
                or relative.as_posix().startswith(SKETCH + "/sdcard/Factory/Tab5OS-0.8.28/")
                or relative.as_posix().startswith("tools/verification/")
                or relative.as_posix() == "LICENCES-manifest.json"):
            continue
        actual_paths.add(relative.as_posix())
    require(actual_paths == inventory_paths, "Publication file set changed")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    require(not output.is_relative_to(root), "Archive must be outside the release tree")
    validate(root)
    omitted_dirs = {".git", "__pycache__", "build-environment", "node_modules"}
    omitted_suffixes = {".pyc", ".pyo", ".obj", ".o", ".exe", ".pdb", ".ilk"}
    old_factory = SKETCH + "/sdcard/Factory/Tab5OS-0.8.28/"
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".partial")
    files = 0
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for path in sorted(root.rglob("*")):
                relative = path.relative_to(root)
                if (not path.is_file() or any(p in omitted_dirs for p in relative.parts)
                        or path.suffix.lower() in omitted_suffixes
                        or relative.as_posix().startswith(old_factory)):
                    continue
                archive.write(path, root.name + "/" + relative.as_posix())
                files += 1
        with zipfile.ZipFile(temporary) as archive:
            require(archive.testzip() is None, "Archive CRC verification failed")
            # Validate the bytes actually archived, including all source,
            # image, test and publication hashes, before replacing the output.
            with tempfile.TemporaryDirectory(prefix="tab5-archive-0829-", dir=output.parent) as check:
                archive.extractall(check)
                validate(Path(check) / root.name)
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)
    print(json.dumps({"file": str(output), "files": files, "bytes": output.stat().st_size,
                      "sha256": digest(output), "version": VERSION, "verified": True}))


if __name__ == "__main__":
    main()
