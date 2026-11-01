"""Package 0.8.32 only after checking its own build and regression evidence.

Historical 0.8.31 reports and binaries cannot satisfy these checks.
This script does not compile, flash, or publish anything.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import tempfile
import zipfile

VERSION = "0.8.32"
SKETCH = "Tab5OS_Arduino_v0.8.32_Ecriture"
# Host-render intermediates are not firmware or application sources.
GENERATED_SKETCH_PREFIX = "tools/native-apps/image-editor/test-output/"
GENERATED_RELEASE_PREFIX = SKETCH + "/" + GENERATED_SKETCH_PREFIX


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
            "The package must contain exactly the 0.8.32 sketch")
    report = read(proofs / "firmware-verification-0832.json")
    require(report["version"] == VERSION and report["passed"] is True and
            report["buildExitCode"] == 0 and report["sourcesUnchanged"] is True,
            "Missing successful 0.8.32 firmware verification")
    build = read(proofs / "build-0832/build-result.json")
    require(build["exitCode"] == 0 and build["sourcesUnchanged"] is True,
            "Full compilation failed or source files changed")
    manifest = read(proofs / "build-0832/source-manifest-end.json")["files"]
    require(read(proofs / "build-0832/source-manifest-start.json")["files"] == manifest,
            "The sources changed during compilation")
    actual = []
    for path in sorted(sketch.rglob("*")):
        if (path.is_file() and path.relative_to(sketch).parts[0] != "sdcard"
                and not path.relative_to(sketch).as_posix().startswith(GENERATED_SKETCH_PREFIX)):
            actual.append({"path": path.relative_to(sketch).as_posix(),
                           "bytes": path.stat().st_size, "sha256": digest(path)})
    require([row for row in manifest if not row["path"].startswith(GENERATED_SKETCH_PREFIX)] == actual,
            "The packaged sources differ from the compiled sources")
    evidence = report["evidenceSha256"]
    require(isinstance(evidence, dict) and
            {"build-result.json", "source-manifest-start.json", "source-manifest-end.json",
             "compile-args.json", "build.log"}.issubset(evidence),
            "Incomplete build evidence hashes")
    for name, sha in evidence.items():
        require(digest(safe_path(proofs / "build-0832", name)) == sha,
                "Build evidence changed: " + name)
    for name in ("compatibility-0832.json", "autosave-battery-0832.json", "git-memory-0832.json", "git-engine-0832.json", "explorer-git-0832.json", "vault-keyboard-0832.json", "abi6-0832.json", "image-editor-0832.json", "image-editor-bmp32-0832.json", "maps-0832.json", "memory-0832.json", "audio-worker-0832.json"):
        test = read(proofs / name)
        require(test["version"] == VERSION and test["passed"] is True,
                "Regression test did not pass: " + name)
        require(bool(test["sourceHashes"]), "Empty test source evidence: " + name)
        for relative, sha in test.get("integrationSourceHashes", {}).items():
            require(digest(safe_path(root, relative)) == sha, "UI integration changed: " + relative)
        for relative, sha in test["sourceHashes"].items():
            require(digest(safe_path(root, relative)) == sha,
                    "Tested source or fixture changed: " + relative)
    # Bind UI integration and native ELF evidence independently of the firmware.
    vault = read(proofs / "vault-keyboard-0832.json")
    for relative, sha in vault["integrationSourceHashes"].items():
        require(digest(safe_path(root, relative)) == sha, "Vault integration changed: " + relative)
    editor = read(proofs / "image-editor-0832.json")
    core_path = safe_path(root, editor["core"]["report"])
    require(digest(core_path) == editor["core"]["reportSha256"], "Image core report changed")
    core = read(core_path)
    require(core["release"] == VERSION and core["status"] == "passed", "Image core tests failed")
    for relative, sha in core["sourceHashes"].items():
        require(digest(safe_path(root, relative)) == sha, "Image core evidence changed: " + relative)
    elf = editor["elf"]
    application = safe_path(root, elf["path"])
    require(elf["abi"] == 6 and elf["imports"] == ["tab5_app_api_v6"] and
            elf["validator"]["accepted"] is True and elf["rebuildByteIdentical"] is True,
            "Missing successful ABI6 ELF verification/rebuild")
    require(application.stat().st_size == elf["bytes"] and digest(application) == elf["sha256"],
            "Delivered image editor differs from tested ELF")
    for group in ("evidenceHashes", "screenshots"):
        for relative, sha in editor[group].items():
            require(digest(safe_path(root, relative)) == sha, "Image editor proof changed: " + relative)
    for name in ("abi6-review-0832.json", "image-review-0832.json"):
        review = read(proofs / name)
        require(review["version"] == VERSION and bool(review["sourceHashes"]), "Missing current review: " + name)
        for relative, sha in review["sourceHashes"].items():
            require(digest(safe_path(root, relative)) == sha, "Reviewed source changed: " + relative)
    # This integration block is inspected, not executed by the manager harness.
    # Bind its exact normalized text using the same extraction as that runner.
    memory = read(proofs / "memory-0832.json")
    main_source = (sketch / (SKETCH + ".ino")).read_text(encoding="utf-8")
    registration = main_source.split("static void registerResourceCaches() {", 1)[1].split("\nvoid setup()", 1)[0]
    require(hashlib.sha256(registration.encode()).hexdigest() ==
            memory["integrationRegistrationSha256"],
            "Resource callback registration changed after memory validation")
    languages = read(proofs / "languages-0832.json")
    require(languages["release"] == VERSION and languages["passed"] is True,
            "Language validation did not pass")
    for language, row in languages["packs"].items():
        require(digest(sketch / "sdcard/System/languages" / (language + ".json")) == row["sha256"],
                "Language changed after validation: " + language)
    resources = read(proofs / "resources-0832.json")
    require(resources["version"] == VERSION and resources["result"] == "PASS",
            "Resource validation did not pass")
    require({SKETCH + "/Tab5SystemResourcesData.h",
             SKETCH + "/sdcard/Factory/Tab5OS-0.8.32/manifest.json"}.issubset(resources["sourceHashes"]),
            "Missing resource fingerprints")
    for relative, sha in resources["sourceHashes"].items():
        require(digest(safe_path(root, relative)) == sha,
                "Resource payload changed after validation: " + relative)
    sd = sketch / "sdcard"
    factory = sd / "Factory/Tab5OS-0.8.32"
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
    require({p.name for p in (root / "firmware").glob("*.bin")} == {"Tab5OS_v0.8.32.bin"},
            "Obsolete or unexpected firmware image")
    expected_debug = {SKETCH + ".ino.elf", SKETCH + ".ino.map"}
    require({p.name for p in (root / "firmware/debug").iterdir() if p.is_file()} == expected_debug,
            "Unexpected firmware diagnostic artifact")
    expected_sha = by_suffix[".bin"]["sha256"] + "  Tab5OS_v0.8.32.bin"
    require((root / "firmware/SHA256.txt").read_text().strip() == expected_sha,
            "OTA SHA256 file differs from verified image")
    publication = read(proofs / "publication-0832.json")
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
                or relative.as_posix().startswith(SKETCH + "/sdcard/Factory/Tab5OS-0.8.31/")
                or relative.as_posix().startswith(GENERATED_RELEASE_PREFIX)
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
    old_factory = SKETCH + "/sdcard/Factory/Tab5OS-0.8.31/"
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".partial")
    files = 0
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for path in sorted(root.rglob("*")):
                relative = path.relative_to(root)
                if (not path.is_file() or any(p in omitted_dirs for p in relative.parts)
                        or path.suffix.lower() in omitted_suffixes
                        or relative.as_posix().startswith(old_factory)
                        or relative.as_posix().startswith(GENERATED_RELEASE_PREFIX)):
                    continue
                archive.write(path, root.name + "/" + relative.as_posix())
                files += 1
        with zipfile.ZipFile(temporary) as archive:
            require(archive.testzip() is None, "Archive CRC verification failed")
            # Validate the bytes actually archived, including all source,
            # image, test and publication hashes, before replacing the output.
            with tempfile.TemporaryDirectory(prefix="tab5-archive-0832-", dir=output.parent) as check:
                archive.extractall(check)
                validate(Path(check) / root.name)
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)
    print(json.dumps({"file": str(output), "files": files, "bytes": output.stat().st_size,
                      "sha256": digest(output), "version": VERSION, "verified": True}))


if __name__ == "__main__":
    main()
