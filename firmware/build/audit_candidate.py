"""Independent, read-only audit of the compiled b224223 complete candidate."""
from pathlib import Path, PurePosixPath
import argparse
import ast
import hashlib
import io
import json
import re
import stat
import subprocess
import zipfile
import zlib

COMMIT = "b224223535a81efdd0076639d77827ae4725b9d7"
BUILD_SHA = "364896cfbeaf2dc445c29b1dda8b5b302e3485f7704b786f150b07ee8154e523"
BUILD_RUN = "42528984471"
BASE_SHA = "d0a3d7987de2d1feb5aa91a1ee6ce1dc10170c0b453f63f7dd9438678e780463"
VERSION = "0.8.53"
SKETCH = "firmware/Tab5OS_Arduino_v0.8.53/"
SD = SKETCH + "sdcard/"
FACTORY = "Factory/Tab5OS-0.8.53/"
PREFIX = "Tab5OS_0.8.53_b224223_complete/"
LARGE = {
    "Games/GB/Tab5_Pocket.gb", "Games/GBC/Tab5_Color.gbc",
    "System/fonts/cjk-ja.ttf", "System/fonts/cjk-ko.ttf", "System/fonts/cjk-zh.ttf",
    "System/ime/ja-kana.dat", "System/ime/sources/SKK-JISYO.L.gz",
    "System/ime/zh-pinyin.dat", "System/maps/gazetteer/cities.t5geo",
}
APPS = {"Doom", "GBA", "Maze3D", "MegaDrive", "NES", "Pentest", "SNES",
        "Sega8", "gameboy", "gameboy_color", "hash_file"}
checks = 0


def require(value, message):
    global checks
    checks += 1
    if not value:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def stream_sha(stream):
    value = hashlib.sha256()
    while True:
        block = stream.read(1024 * 1024)
        if not block:
            return value.hexdigest()
        value.update(block)


def file_sha(path):
    with path.open("rb") as stream:
        return stream_sha(stream)


def safe(name):
    p = PurePosixPath(name)
    require(bool(name) and not p.is_absolute() and ".." not in p.parts and
            "\\" not in name and ":" not in name and p.as_posix() == name,
            "Unsafe archive path: " + name)


def inventory(archive):
    names = []
    for item in archive.infolist():
        safe(item.filename.rstrip("/") if item.is_dir() else item.filename)
        require(not stat.S_ISLNK(item.external_attr >> 16), "Archive symlink: " + item.filename)
        if not item.is_dir():
            names.append(item.filename)
    require(len(names) == len(set(names)), "Duplicate ZIP members")
    require(len(names) == len({n.casefold() for n in names}), "Case-colliding ZIP members")
    require(archive.testzip() is None, "ZIP CRC failure")
    return set(names)


def sums(raw):
    values = {}
    for line in raw.decode("utf8").splitlines():
        require(re.fullmatch(r"[0-9a-f]{64}  .+", line) is not None, "Malformed checksum row")
        digest, name = line.split("  ", 1)
        safe(name)
        require(name not in values, "Duplicate checksum name: " + name)
        values[name] = digest
    return values


def check_sums(archive, prefix, names):
    values = sums(archive.read(prefix + "SHA256SUMS.txt"))
    require(set(values) == names - {"SHA256SUMS.txt"}, "Checksum file is not an exact inventory")
    for name, expected in values.items():
        with archive.open(prefix + name) as stream:
            require(stream_sha(stream) == expected, "Checksum mismatch: " + name)
    return values


def assignments(raw):
    values = {}
    wanted = {"VERSION", "FILES", "PREVIOUS"}
    for node in ast.parse(raw.decode("utf8")).body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            if isinstance(target, ast.Name) and target.id in wanted:
                values[target.id] = ast.literal_eval(node.value)
    require(set(values) == wanted, "Factory literal metadata is incomplete")
    return values


def valid_wad_header(raw, size):
    if len(raw) < 12 or raw[:4] not in (b"IWAD", b"PWAD"):
        return False
    count = int.from_bytes(raw[4:8], "little")
    offset = int.from_bytes(raw[8:12], "little")
    return count <= 100000 and 12 <= offset <= size and offset + count * 16 <= size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "artifact", "package", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    require(subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=args.repo).decode().strip() == COMMIT,
            "Source checkout is not the validated commit")
    require(not subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=args.repo).strip(),
            "Validated source checkout has tracked modifications")
    artifact_sha = file_sha(args.artifact)
    require(artifact_sha == BUILD_SHA, "Build artifact does not match GitHub digest")
    baseline_path = args.repo / "downloads/Tab5OS_0.8.53_SD.zip"
    require(file_sha(baseline_path) == BASE_SHA, "Baseline SD archive changed")
    with zipfile.ZipFile(args.package) as package, zipfile.ZipFile(args.artifact) as ci, zipfile.ZipFile(baseline_path) as baseline:
        members = inventory(package)
        ci_members = inventory(ci)
        inventory(baseline)
        require(all(n.startswith(PREFIX) for n in members), "Unexpected package root")
        names = {n[len(PREFIX):] for n in members}
        package_sums = check_sums(package, PREFIX, names)
        read = lambda name: package.read(PREFIX + name)
        info = json.loads(read("BUILD_INFO.json"))
        require(info["validatedCommit"] == COMMIT, "BUILD_INFO commit differs")
        require(info["workflowRun"].rstrip("/").endswith("/" + BUILD_RUN), "Build workflow differs")
        require(info["artifactSha256"] == BUILD_SHA, "BUILD_INFO artifact digest differs")
        require(ci.read("qa/validated-commit.txt").decode().strip() == COMMIT, "CI commit marker differs")
        for name in ci_members:
            if name.startswith(("qa/", "native/")):
                require(read(name) == ci.read(name), "CI evidence/native payload differs: " + name)
        firmware = json.loads(ci.read("qa/firmware-verification-0853.json"))
        for flag in ("passed", "sourcesUnchanged", "imageChecksumValid", "imageValidationHashValid",
                     "elfEmbeddedShaMatches", "elf32LittleEndian", "singleFloatAbi",
                     "partitionsMatchExpectedLayout", "appVersionPresent"):
            require(firmware[flag] is True, "Firmware verification failed: " + flag)
        require(firmware["buildExitCode"] == 0 and firmware["imageChip"] == "ESP32-P4", "Wrong firmware build/target")
        require(firmware["version"] == info["version"] == VERSION, "Version mismatch")
        payloads = {}
        for item in firmware["artifacts"]:
            raw = ci.read("build/" + item["file"])
            require(len(raw) == item["bytes"] and sha(raw) == item["sha256"], "CI firmware checksum mismatch")
            destination = ("downloads/Tab5OS_0.8.53_OTA.bin" if item["suffix"] == ".bin" else
                           "downloads/Tab5OS_0.8.53_initial_install_16MB.bin" if item["suffix"] == ".merged.bin" else
                           "qa/build/" + item["file"])
            require(read(destination) == raw, "Packaged firmware differs from CI: " + destination)
            payloads[item["suffix"]] = raw
        require(set(payloads) == {".bin", ".merged.bin", ".elf", ".map", ".bootloader.bin", ".partitions.bin"},
                "Firmware component set differs")
        require(len(payloads[".bin"]) == firmware["firmwareBytes"] == info["firmwareBytes"] == 6126656,
                "Firmware size differs")
        require(sha(payloads[".bin"]) == info["firmwareSha256"], "Firmware BUILD_INFO checksum differs")
        require(info["otaPartitionBytes"] == firmware["otaPartitionBytes"] == 6553600, "OTA partition size differs")
        require(info["otaFreeBytes"] == firmware["otaPartitionBytes"] - len(payloads[".bin"]) == 426944,
                "OTA free space differs")
        require(len(payloads[".merged.bin"]) == 16 * 1024 * 1024, "First-install image size differs")
        for offset, raw in ((0x2000, payloads[".bootloader.bin"]), (0x8000, payloads[".partitions.bin"]),
                            (0xe000, ci.read("build/boot_app0.bin")), (0x10000, payloads[".bin"])):
            require(payloads[".merged.bin"][offset:offset + len(raw)] == raw, "Merged image component differs")
        for name in ("boot_app0.bin", "partitions.csv", "sdkconfig"):
            require(read("qa/build/" + name) == ci.read("build/" + name), "Build support file differs")
        for name, digest in firmware["evidenceSha256"].items():
            safe(name)
            require(sha(read("qa/" + name)) == digest, "Firmware evidence checksum differs: " + name)
        source_manifest = json.loads(ci.read("qa/source-manifest-final.json"))["files"]
        require(len(source_manifest) == len({item["path"] for item in source_manifest}), "Duplicate source manifest path")
        for item in source_manifest:
            safe(item["path"])
            name = SKETCH + item["path"]
            raw = read(name)
            require(len(raw) == item["bytes"] and sha(raw) == item["sha256"], "Packaged built source differs: " + name)
            require(raw == (args.repo / name).read_bytes(), "Checkout differs from built source: " + name)
        require(len(source_manifest) == info["sourceFilesMatchedToBuild"], "Source count differs")
        tracked = set(filter(None, subprocess.check_output(["git", "ls-files", "-z"], cwd=args.repo).decode().split("\0")))
        for name in tracked:
            if not name.startswith(("downloads/", SD)) and name != "README.md":
                require(read(name) == (args.repo / name).read_bytes(), "Tracked source/guide differs: " + name)
        require(read("README.md").endswith((args.repo / "README.md").read_bytes()), "Repository README content missing")
        downloads = {n.removeprefix("downloads/") for n in names if n.startswith("downloads/") and n != "downloads/SHA256SUMS.txt"}
        download_sums = sums(read("downloads/SHA256SUMS.txt"))
        require(downloads == set(download_sums) == {"Tab5OS_0.8.53_OTA.bin", "Tab5OS_0.8.53_initial_install_16MB.bin", "Tab5OS_0.8.53_SD.zip"},
                "Install download manifest differs")
        for name, digest in download_sums.items():
            require(sha(read("downloads/" + name)) == digest, "Download checksum differs")
        with zipfile.ZipFile(io.BytesIO(read("downloads/Tab5OS_0.8.53_SD.zip"))) as sd:
            sd_members = inventory(sd)
            check_sums(sd, "", sd_members)
            nested = {n[len("sdcard/"):] for n in sd_members if n.startswith("sdcard/")}
            extracted = {n[len(SD):] for n in names if n.startswith(SD)}
            require(nested == extracted, "Nested and extracted SD file sets differ")
            require(len(nested) == info["sdFilesIncludingFactory"], "SD file count differs")
            for name in nested:
                raw = sd.read("sdcard/" + name)
                require(raw == read(SD + name), "Nested and extracted SD bytes differ: " + name)
                require(not name.lower().endswith(".wad") and not valid_wad_header(raw[:12], len(raw)),
                        "Game/test WAD bundled: " + name)
            live = {name for name in nested if not name.startswith("Factory/")}
            tracked_sd = {name[len(SD):] for name in tracked if name.startswith(SD)}
            require(live - tracked_sd == LARGE, "Unexpected baseline-only SD assets")
            require(set(info["unchangedLargeAssetsFromBaseline"]) == LARGE, "Baseline asset provenance differs")
            for name in LARGE:
                require(sd.read("sdcard/" + name) == baseline.read("sdcard/" + name), "Large baseline asset differs: " + name)
            for name in tracked_sd - {"Applications/Doom.app.elf"}:
                require(sd.read("sdcard/" + name) == (args.repo / SD / name).read_bytes(), "Tracked SD content differs: " + name)
            factory = json.loads(sd.read("sdcard/" + FACTORY + "manifest.json"))
            rules = assignments(sd.read("sdcard/MicroPython/lib/tab5factory.py"))
            require(factory["version"] == rules["VERSION"] == VERSION, "Factory version differs")
            require(len(factory["files"]) == len(rules["FILES"]) == info["factoryFiles"] == 180,
                    "Factory must contain exactly 180 managed files")
            require(set(factory["files"]) == set(rules["FILES"]), "Factory managed sets differ")
            require({n for n in nested if n.startswith(FACTORY)} ==
                    {FACTORY + n for n in factory["files"]} | {FACTORY + "manifest.json"}, "Factory inventory differs")
            old_factory = json.loads(baseline.read("sdcard/" + FACTORY + "manifest.json"))
            for name, digest in factory["files"].items():
                require(sha(sd.read("sdcard/" + name)) == digest, "Live resource differs from Factory hash: " + name)
                require(sha(sd.read("sdcard/" + FACTORY + name)) == digest, "Factory resource hash differs: " + name)
                old = old_factory["files"][name]
                if old != digest:
                    require(old in rules["PREVIOUS"].get(name, ()), "Old official resource missing from repair history: " + name)
            previous_doom = "71918702f7c583a519577b0660258e6f48a4b82d0e542233b90e02754158135b"
            require(previous_doom in rules["PREVIOUS"]["Applications/Doom.app.elf"], "Previous candidate Doom is not recognized for update")
            apps = {name for name in live if name.startswith("Applications/") and name.endswith(".app.elf")}
            require(apps == {"Applications/" + app + ".app.elf" for app in APPS}, "Application set differs")
            require(len(apps) == info["nativeApplications"] == 11, "Application count differs")
            app_hashes = {}
            for name in sorted(apps):
                raw = sd.read("sdcard/" + name)
                require(raw[:6] == b"\x7fELF\x01\x01" and int.from_bytes(raw[16:18], "little") == 3 and
                        int.from_bytes(raw[18:20], "little") == 243, "Invalid RISC-V ELF32 DYN application: " + name)
                app_hashes[name] = sha(raw)
            doom = ci.read("native/Doom.app.elf")
            require(sd.read("sdcard/Applications/Doom.app.elf") == doom, "Packaged Doom differs from CI")
            doom_report = json.loads(ci.read("qa/doom-build-report.json"))
            require(doom_report["passed"] is True and sha(doom) == doom_report["artifact"]["sha256"] == info["doomSha256"],
                    "Doom verification/checksum differs")
            require(len(doom) == doom_report["artifact"]["bytes"] == info["doomBytes"], "Doom byte count differs")
            for name, digest in doom_report["sourceHashes"].items():
                require(sha(read(name)) == digest, "Doom source differs from verified build: " + name)
            header = read(SKETCH + "Tab5SystemResourcesData.h").decode("utf8")
            arrays = {symbol: bytes.fromhex("".join(re.findall(r"0x([0-9a-fA-F]{2})", body)))
                      for symbol, body in re.findall(r"static const uint8_t (TAB5_RESOURCE_\d+)\[\] = \{(.*?)\};", header, re.S)}
            seeds = re.findall(r'\{"(/[^"\n]+)", (TAB5_RESOURCE_\d+), sizeof\(\2\), (\d+)\}', header)
            require(len(seeds) == info["embeddedResourcesMatched"] == 40, "Embedded resource count differs")
            for name, symbol, size in seeds:
                raw = zlib.decompress(arrays[symbol])
                require(len(raw) == int(size) and raw == sd.read("sdcard" + name), "Embedded seed differs: " + name)
            require(sd.read("LICENCES.md") == read("firmware/LICENCES.md"), "Nested license guide differs")
            licenses = json.loads(sd.read("LICENCES-manifest.json"))
            require(licenses == json.loads(read("firmware/LICENCES-manifest.json")), "Nested license manifest differs")
            for component in licenses["components"]:
                for notice in component["notices"]:
                    require(sha(sd.read("notices/" + notice["path"])) == notice["sha256"], "License notice differs")
        for name in names:
            require(not name.lower().endswith(".wad"), "WAD file bundled: " + name)
            with package.open(PREFIX + name) as stream:
                require(not valid_wad_header(stream.read(12), package.getinfo(PREFIX + name).file_size),
                        "WAD payload bundled under another name: " + name)
        result = {
            "passed": True, "checks": checks, "validatedCommit": COMMIT, "buildRun": BUILD_RUN,
            "artifactSha256": artifact_sha, "packageSha256": file_sha(args.package),
            "packageBytes": args.package.stat().st_size, "exactManifestFiles": len(package_sums),
            "sourceFilesMatchedToCi": len(source_manifest), "sdFilesIncludingFactory": len(nested),
            "factoryFiles": 180, "nativeApplications": app_hashes, "baselineAssets": sorted(LARGE),
            "embeddedResourcesMatched": len(seeds), "firmwareSha256": info["firmwareSha256"],
            "firmwareBytes": info["firmwareBytes"], "otaFreeBytes": info["otaFreeBytes"],
            "testWadBundled": False, "inspectorSha256": file_sha(Path(__file__)),
            "limitations": ["Archive and build consistency verified; no physical Tab5 test."],
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
