"""Package the current release only after checking its own build and regression evidence.

Current changed-code gates require current executions. Unchanged engines are
checked byte-for-byte against the authenticated delivered baseline;
their historical reports retain their original version and are not re-run.
This script does not compile, flash, or publish anything.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import tempfile
import zipfile

from release_config import VERSION, SKETCH, TAG
from release_artifacts import copy_temporary
# Host-render intermediates are not firmware or application sources.
GENERATED_SKETCH_PREFIX = "tools/native-apps/image-editor/test-output/"
GENERATED_RELEASE_PREFIX = SKETCH + "/" + GENERATED_SKETCH_PREFIX
FACTORY_PREFIX = SKETCH + "/sdcard/Factory/"


def obsolete_factory(relative):
    return relative.startswith(FACTORY_PREFIX) and not relative.startswith(FACTORY_PREFIX + "Tab5OS-" + VERSION + "/")


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
            f"The package must contain exactly the {VERSION} sketch")
    report = read(proofs / f"firmware-verification-{TAG}.json")
    require(report["version"] == VERSION and report["passed"] is True and
            report["buildExitCode"] == 0 and report["sourcesUnchanged"] is True,
            f"Missing successful {VERSION} firmware verification")
    build = read(proofs / f"build-{TAG}/build-result.json")
    require(build["exitCode"] == 0 and build["sourcesUnchanged"] is True,
            "Full compilation failed or source files changed")
    manifest = read(proofs / f"build-{TAG}/source-manifest-end.json")["files"]
    require(read(proofs / f"build-{TAG}/source-manifest-start.json")["files"] == manifest,
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
        require(digest(safe_path(proofs / f"build-{TAG}", name)) == sha,
                "Build evidence changed: " + name)
    downloads = read(proofs / f"build-{TAG}/dependency-downloads.json")
    require(downloads["passed"] is True and downloads["cli"]["matchesExpected"] is True and
            len(downloads["dependencies"]) == 23 and
            all(row["matchesExpected"] is True for row in downloads["dependencies"]),
            "Pinned dependency archive verification did not pass")
    require(downloads["lockSha256"] == digest(root / "tools/build_linux/dependency-downloads.json") and
            downloads["verifierSha256"] == digest(root / "tools/build_linux/verify_downloads.py"),
            "Dependency lock or verifier changed after verification")
    audio_backport = report.get("m5UnifiedAudioBackport", {})
    require(audio_backport.get("passed") is True and
            audio_backport.get("unpatchedDependencyUsers") == [] and
            len(audio_backport.get("compiledBindings", [])) == 1 and
            len(audio_backport.get("patch", {}).get("files", [])) == 3,
            "Isolated M5Unified audio source/link verification missing")
    # Current reports bind actual compilation, host execution or explicitly
    # scoped structural checks. Historical reports cannot satisfy this gate.
    scope = read(root / "tools/release_scope.json")
    require(scope["version"] == VERSION and scope.get("finalized") is True,
            "Current test/source scope is not finalized")
    require(bool(scope.get("targetedReports")), "No current regression gates")
    for row in scope["targetedReports"]:
        name = row["file"]
        test = read(proofs / name)
        require(test.get("version", test.get("release")) == VERSION and
                (test.get("passed") is True or test.get("result") == "PASS"),
                "Targeted regression did not pass: " + name)
        require(bool(test.get("sourceHashes")), "Empty test source evidence: " + name)
        for group in ("sourceHashes", "testSourceHashes", "testHashes", "integrationSourceHashes", "evidenceHashes", "screenshots"):
            for relative, sha in test.get(group, {}).items():
                require(digest(safe_path(root, relative)) == sha,
                        "Tested source or evidence changed: " + relative)
    from check_preserved import validate as check_preserved
    preserved = check_preserved(root)
    recorded = read(proofs / f"preserved-components-{TAG}.json")
    require(recorded["passed"] is True and recorded["filesChecked"] == preserved["filesChecked"] and
            recorded["baselineManifestSha256"] == preserved["baselineManifestSha256"] and
            recorded["checkerSha256"] == preserved["checkerSha256"] and
            recorded["historicalReportHashes"] == preserved["historicalReportHashes"] and
            recorded.get("excludedHistoricalCopyArtifacts") == preserved["excludedHistoricalCopyArtifacts"] and
            recorded["sourceChangeScopeSha256"] == preserved["sourceChangeScopeSha256"] and
            recorded["changedSources"] == preserved["changedSources"],
            "Preserved component evidence changed")
    decoder_sha = report["decoderSourceSha256"]
    for name in ("image-codec-0835.json", "image-native-0835.json"):
        require(read(proofs / name)["decoderSourceSha256"] == decoder_sha,
                "Preserved historical/firmware JPEG decoder mismatch: " + name)
    languages = read(proofs / f"languages-{TAG}.json")
    require(languages["release"] == VERSION and languages["passed"] is True,
            "Language validation did not pass")
    for language, row in languages["packs"].items():
        require(digest(sketch / "sdcard/System/languages" / (language + ".json")) == row["sha256"],
                "Language changed after validation: " + language)
    localization = read(proofs / f"evolutions-localization-{TAG}.json")
    require(localization["version"] == VERSION and localization["passed"] is True and
            localization["baselineMetadataPreserved"] is True and
            localization["baselineTranslationsPreserved"] is True and
            localization["sourceKeyCoverage"]["passed"] is True,
            "Current UI translations were not verified")
    # .53 adds six reviewed labels while preserving all existing entries.
    require(localization["reviewedAdditionsPerPack"] == 6 and
            len(localization["packs"]) == 14 and
            all(len(row["reviewedAddedKeys"]) == 6 and row["baselineEntriesPreserved"] > 0
                for row in localization["packs"].values()),
            "Unexpected localization delta")
    require(bool(localization.get("sourceHashes")), "Missing localization source fingerprints")
    for relative, sha in localization["sourceHashes"].items():
        require(digest(safe_path(root, relative)) == sha,
                "Localized source changed after coverage audit: " + relative)
    resources = read(proofs / f"resources-{TAG}.json")
    require(resources["version"] == VERSION and resources["result"] == "PASS",
            "Resource validation did not pass")
    require({SKETCH + "/Tab5SystemResourcesData.h", SKETCH + "/Tab5EnglishFallback.h", SKETCH + "/Tab5EnglishBitmap.h",
             SKETCH + f"/sdcard/Factory/Tab5OS-{VERSION}/manifest.json"}.issubset(resources["sourceHashes"]),
            "Missing resource fingerprints")
    for relative, sha in resources["sourceHashes"].items():
        require(digest(safe_path(root, relative)) == sha,
                "Resource payload changed after validation: " + relative)
    sd = sketch / "sdcard"
    factory = sd / f"Factory/Tab5OS-{VERSION}"
    factory_manifest = read(factory / "manifest.json")
    require(factory_manifest["version"] == VERSION, "Wrong Factory version")
    from rebuild_factory import RESOURCE_RENAMES
    require(all(not (sd / previous).exists() and (sd / current).is_file()
                for previous, current in RESOURCE_RENAMES.items()),
            "Legacy French resource name remains or English resource is missing")
    require({path.relative_to(factory).as_posix() for path in factory.rglob("*") if path.is_file()} ==
            set(factory_manifest["files"]) | {"manifest.json"}, "Unexpected file in current Factory")
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
    require({p.name for p in (root / "firmware").glob("*.bin")} == {f"Tab5OS_v{VERSION}.bin"},
            "Obsolete or unexpected firmware image")
    expected_debug = {SKETCH + ".ino.elf", SKETCH + ".ino.map"}
    actual_debug = {p.name for p in (root / "firmware/debug").iterdir() if p.is_file()}
    require(actual_debug == expected_debug,
            "Unexpected firmware diagnostic artifact: " + repr(sorted(actual_debug ^ expected_debug)))
    require({p.relative_to(root / "firmware").as_posix() for p in (root / "firmware").rglob("*") if p.is_file()} ==
            {f"Tab5OS_v{VERSION}.bin", "SHA256.txt", "debug/" + SKETCH + ".ino.elf", "debug/" + SKETCH + ".ino.map"},
            "Firmware directory must contain exactly the verified BIN, SHA256, ELF and map")
    expected_sha = by_suffix[".bin"]["sha256"] + f"  Tab5OS_v{VERSION}.bin"
    require((root / "firmware/SHA256.txt").read_text().strip() == expected_sha,
            "OTA SHA256 file differs from verified image")
    publication = read(proofs / f"publication-{TAG}.json")
    require(publication["version"] == VERSION and publication["passed"] is True,
            "Publication inventory did not pass")
    require(digest(root / "tools/audit_publication_current.py") == publication["auditScriptSha256"],
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
                or obsolete_factory(relative.as_posix())
                or copy_temporary(relative.as_posix())
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
    # Freeze the exact four validated firmware names. A workspace copy can
    # create temporary files after validate(); none is a release artifact.
    firmware_files = {"firmware/" + f"Tab5OS_v{VERSION}.bin", "firmware/SHA256.txt",
                      "firmware/debug/" + SKETCH + ".ino.elf",
                      "firmware/debug/" + SKETCH + ".ino.map"}
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".partial")
    files = 0
    archive_root = f"Tab5OS_Ecriture_v{VERSION}"
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for path in sorted(root.rglob("*")):
                relative = path.relative_to(root)
                if (not path.is_file() or any(p in omitted_dirs for p in relative.parts)
                        or path.suffix.lower() in omitted_suffixes
                        or obsolete_factory(relative.as_posix())
                or copy_temporary(relative.as_posix())
                        or relative.as_posix().startswith(GENERATED_RELEASE_PREFIX)
                        or (relative.parts[0] == "firmware" and relative.as_posix() not in firmware_files)):
                    continue
                archive.write(path, archive_root + "/" + relative.as_posix())
                files += 1
        with zipfile.ZipFile(temporary) as archive:
            require(archive.testzip() is None, "Archive CRC verification failed")
            require({n[len(archive_root)+1:] for n in archive.namelist()
                     if n.startswith(archive_root + "/firmware/")} == firmware_files,
                    "Archive firmware inventory differs from verified artifacts")
            # Validate the bytes actually archived, including all source,
            # image, test and publication hashes, before replacing the output.
            with tempfile.TemporaryDirectory(prefix=f"tab5-archive-{TAG}-", dir=output.parent) as check:
                archive.extractall(check)
                validate(Path(check) / archive_root)
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)
    print(json.dumps({"file": str(output), "files": files, "bytes": output.stat().st_size,
                      "sha256": digest(output), "version": VERSION, "verified": True}))


if __name__ == "__main__":
    main()
