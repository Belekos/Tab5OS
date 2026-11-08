#!/usr/bin/env python3
"""Stage a complete Tab5OS candidate from one pinned, verified CI artifact.

The output is a directory tree for actions/upload-artifact. Its downloaded ZIP
is the final complete package; only the SD-card distribution is a nested ZIP.
No repository or device changes are performed.
"""
from pathlib import Path, PurePosixPath
import argparse
import ast
import hashlib
import json
import re
import shutil
import subprocess
import zipfile
import zlib

VERSION = "0.8.53"
COMMIT = "b224223535a81efdd0076639d77827ae4725b9d7"
TREE = "fdbfa7b1997cd4b4ef0eb69c06d7611b3ce27ef5"
PREVIOUS_COMMIT = "8d8d4f6865b1016de5c45910d604d2055bf894c2"
PREVIOUS_DOOM = "71918702f7c583a519577b0660258e6f48a4b82d0e542233b90e02754158135b"
ARTIFACT_SHA = "364896cfbeaf2dc445c29b1dda8b5b302e3485f7704b786f150b07ee8154e523"
BASE_SD_SHA = "d0a3d7987de2d1feb5aa91a1ee6ce1dc10170c0b453f63f7dd9438678e780463"
WORKFLOW = "https://github.com/Belekos/Tab5OS/actions/runs/42528984471"
WAD_SHA = "1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771"
SKETCH = "Tab5OS_Arduino_v" + VERSION
SKETCH_REL = "firmware/" + SKETCH
SD_REL = SKETCH_REL + "/sdcard"
FACTORY = "Factory/Tab5OS-" + VERSION
NAME = "Tab5OS_" + VERSION + "_" + COMMIT[:7] + "_complete"
LARGE_ASSETS = {
    "Games/GB/Tab5_Pocket.gb", "Games/GBC/Tab5_Color.gbc",
    "System/fonts/cjk-ja.ttf", "System/fonts/cjk-ko.ttf", "System/fonts/cjk-zh.ttf",
    "System/ime/ja-kana.dat", "System/ime/sources/SKK-JISYO.L.gz",
    "System/ime/zh-pinyin.dat", "System/maps/gazetteer/cities.t5geo",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def json_bytes(value):
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf8")


def safe_name(value):
    p = PurePosixPath(value)
    require(bool(value) and not p.is_absolute() and ".." not in p.parts
            and "\\" not in value and ":" not in value and p.as_posix() == value,
            "Unsafe archive/source path: " + value)
    return value


def check_archive(z):
    names = [i.filename for i in z.infolist() if not i.is_dir()]
    require(len(names) == len(set(names)), "Duplicate ZIP member")
    require(len(names) == len({n.casefold() for n in names}), "Case-colliding ZIP member")
    for item in z.infolist():
        if item.is_dir():
            continue
        safe_name(item.filename)
        require((item.external_attr >> 16) & 0o170000 != 0o120000, "Symlink in ZIP")
    require(z.testzip() is None, "Corrupt input ZIP")


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def assignments(raw):
    wanted = {"VERSION", "FILES", "PREVIOUS"}
    result = {}
    for node in ast.parse(raw.decode("utf8")).body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            if isinstance(target, ast.Name) and target.id in wanted:
                result[target.id] = ast.literal_eval(node.value)
    require(set(result) == wanted, "Factory module lacks literal resource metadata")
    return result


def verify_sources(repo):
    require(git(repo, "rev-parse", "HEAD").decode().strip() == COMMIT, "Wrong source commit")
    require(git(repo, "rev-parse", "HEAD^{tree}").decode().strip() == TREE, "Wrong source tree")
    require(not git(repo, "status", "--porcelain", "--untracked-files=no").strip(),
            "Source worktree has tracked modifications")
    names = list(filter(None, git(repo, "ls-files", "-z").decode().split("\0")))
    require(len(names) > 1000, "Source checkout is incomplete")
    for name in names:
        safe_name(name)
        path = repo / name
        require(path.is_file() and not path.is_symlink(), "Missing/linked tracked file: " + name)
    return names


def verify_firmware(z, repo):
    require(z.read("qa/validated-commit.txt").decode().strip() == COMMIT, "Wrong CI source commit")
    report = json.loads(z.read("qa/firmware-verification-0853.json"))
    for key in ("passed", "sourcesUnchanged", "imageChecksumValid", "imageValidationHashValid",
                "elfEmbeddedShaMatches", "elf32LittleEndian", "singleFloatAbi",
                "partitionsMatchExpectedLayout", "appVersionPresent"):
        require(report.get(key) is True, "Firmware check failed: " + key)
    require(report["buildExitCode"] == 0 and report["imageChip"] == "ESP32-P4"
            and report["version"] == VERSION, "Wrong firmware target/version")
    payloads = {}
    for item in report["artifacts"]:
        data = z.read("build/" + safe_name(item["file"]))
        require(len(data) == item["bytes"] and sha(data) == item["sha256"],
                "Firmware component mismatch: " + item["file"])
        require(item["suffix"] not in payloads, "Duplicate firmware suffix")
        payloads[item["suffix"]] = data
    for suffix in (".bin", ".merged.bin", ".elf", ".map", ".bootloader.bin", ".partitions.bin"):
        require(suffix in payloads, "Missing firmware component: " + suffix)
    require(len(payloads[".bin"]) == report["firmwareBytes"] == 6126656,
            "Unexpected firmware size")
    require(report["otaFreeBytes"] == report["otaPartitionBytes"] - len(payloads[".bin"]) == 426944,
            "Firmware does not fit expected OTA partition")
    merged = payloads[".merged.bin"]
    require(len(merged) == 16 * 1024 * 1024, "Initial image must be complete 16 MiB")
    for offset, data in ((0x2000, payloads[".bootloader.bin"]),
                         (0x8000, payloads[".partitions.bin"]),
                         (0xe000, z.read("build/boot_app0.bin")), (0x10000, payloads[".bin"])):
        require(merged[offset:offset + len(data)] == data, "Merged image component mismatch")
    for name, digest in report["evidenceSha256"].items():
        require(sha(z.read("qa/" + safe_name(name))) == digest, "CI evidence mismatch: " + name)
    manifest = json.loads(z.read("qa/source-manifest-final.json"))["files"]
    require(len(manifest) > 1000, "Incomplete build source manifest")
    for item in manifest:
        path = repo / SKETCH_REL / safe_name(item["path"])
        require(path.is_file(), "Build source missing: " + item["path"])
        data = path.read_bytes()
        require(len(data) == item["bytes"] and sha(data) == item["sha256"],
                "Build source differs: " + item["path"])
    return report, payloads, len(manifest)


def verify_doom(z, repo):
    raw = z.read("native/Doom.app.elf")
    report = json.loads(z.read("qa/doom-build-report.json"))
    require(report.get("passed") is True, "Doom build did not pass")
    require(sha(raw) == report["artifact"]["sha256"]
            and len(raw) == report["artifact"]["bytes"], "Doom build artifact mismatch")
    require(raw[:6] == b"\x7fELF\x01\x01" and int.from_bytes(raw[18:20], "little") == 243
            and int.from_bytes(raw[16:18], "little") == 3, "Wrong Doom ELF target/type")
    require(len(report["sourceHashes"]) > 100, "Incomplete Doom source manifest")
    for name, digest in report["sourceHashes"].items():
        require(sha((repo / safe_name(name)).read_bytes()) == digest, "Doom source mismatch: " + name)
    return raw, report


def verify_runtime(z, repo, doom, build):
    tests = SKETCH_REL + "/tools/native-apps/doom/tests/"
    result = {}
    for name, frames, play in (("doom-startup-report.json", 120, False),
                               ("doom-play-report.json", 240, True)):
        r = json.loads(z.read("qa/" + name))
        require(r.get("schemaVersion") == 1 and r.get("passed") is True, "Doom emulation failed")
        require(r["stopReason"] == "frame-milestone" and r["error"] is None
                and r["returnCode"] is None, "Doom returned or faulted")
        require(r["frameTarget"] == r["frameCount"] == frames and r["nonzeroFrameCount"] > 0,
                "Doom did not render expected frames")
        require(r["unicornVersion"] == "2.1.4", "Unexpected emulator")
        require(r["elf"]["sha256"] == sha(doom) and r["elf"]["bytes"] == len(doom),
                "Emulation used different ELF")
        require(r["scriptSha256"] == sha((repo / tests / "emulate_startup.py").read_bytes()),
                "Emulation script differs")
        require(r["wad"]["sha256"] == WAD_SHA and r["wad"]["bytes"] == 4196020,
                "Unexpected emulation fixture")
        require(r["playScenario"]["enabled"] is play, "Wrong emulation scenario")
        if play:
            require(r["playScenario"]["levelInitialized"] is True
                    and r["playScenario"]["inputPolls"] > 0 and r["uniqueFrameCount"] > 1,
                    "Gameplay not exercised")
        require(r["apiCallCounts"]["video_present"] == frames
                and r["apiCallCounts"]["video_begin"] == 1
                and r["apiCallCounts"]["audio_pcm"] > 0, "Incomplete OS service exercise")
        require(r["loader"]["imports"] == ["tab5_app_api_v4"]
                and r["loader"]["stackBytes"] == 16384, "Wrong loader/stack setup")
        result[name] = {"passed": True, "frames": frames, "elfSha256": sha(doom),
                        "sha256": sha(z.read("qa/" + name))}
    name = "doom-startup-host.json"
    r = json.loads(z.read("qa/" + name))
    require(r.get("passed") is True and r["component"] == "Complete Doom host startup"
            and r["sanitizers"] == ["AddressSanitizer"] and r["wadSha256"] == WAD_SHA,
            "Doom host validation failed")
    native = SKETCH_REL + "/tools/native-apps/"
    required = {native + p for p in (
        "doom/app.c", "doom/papp_i_system.c", "doom/papp_i_video.c", "doom/papp_i_sound.c",
        "doom/papp_rg_stubs.c", "doom/tests/startup_host.c", "doom/tests/run_startup_host.py",
        "common/runtime.c", "common/game_runtime.c", "common/math.c")}
    require(required <= set(r["sourceHashes"]), "Required Doom host source omitted")
    for path, digest in r["sourceHashes"].items():
        require(sha((repo / safe_name(path)).read_bytes()) == digest
                and build["sourceHashes"].get(path) == digest, "Doom host/build source mismatch")
    expected = {"title": (0, 120), "play": (0, 240), "missing": (9, 0),
                "no-argument": (2, 0), "oom": (9, 0), "video-failure": (3, 0)}
    require(len(r["scenarios"]) == 6 and {x["scenario"] for x in r["scenarios"]} == set(expected),
            "Incomplete host scenarios")
    for case in r["scenarios"]:
        require(case.get("passed") is True
                and (case["result"], case["frames"]) == expected[case["scenario"]],
                "Doom host case failed")
        if case["scenario"] in ("title", "play"):
            require(case["nonzeroFrames"] > 0 and case["audioCalls"] > 0, "Host render/audio absent")
        if case["scenario"] == "play":
            require(case["saves"] > 0, "Host save not exercised")
    result[name] = {"passed": True, "scenarios": 6, "sha256": sha(z.read("qa/" + name))}
    return result


def verify_core(z, repo):
    r = json.loads(z.read("qa/image-core-report.json"))
    require(r["status"] == "passed" and r["release"] == VERSION, "Image core failed")
    require(r["sanitizers"] == ["AddressSanitizer", "UndefinedBehaviorSanitizer"],
            "Image core sanitizers missing")
    require(r["allocation_scenarios"] == 336 and r["repeated_cycles"] == 400,
            "Core regression coverage differs")
    require(r["reliefRegression"]["status"] == "passed"
            and r["reliefRegression"]["allocation_scenarios"] == 48, "Relief regression failed")
    for name, digest in r["sourceHashes"].items():
        require(name.startswith(SKETCH + "/"), "Unexpected core source manifest scope")
        require(sha((repo / "firmware" / safe_name(name)).read_bytes()) == digest,
                "Core test source mismatch: " + name)
    return {"passed": True, "checks": r["checks"], "reliefChecks": r["reliefRegression"]["checks"],
            "sha256": sha(z.read("qa/image-core-report.json"))}


def prepare_sd(repo, tracked, doom):
    baseline = repo / ("downloads/Tab5OS_" + VERSION + "_SD.zip")
    require(sha(baseline.read_bytes()) == BASE_SD_SHA, "Unexpected baseline SD ZIP")
    with zipfile.ZipFile(baseline) as z:
        check_archive(z)
        old = json.loads(z.read("sdcard/" + FACTORY + "/manifest.json"))
        require(old["version"] == VERSION and len(old["files"]) == 180, "Unexpected baseline Factory")
        sd = {i.filename.removeprefix("sdcard/"): z.read(i) for i in z.infolist()
              if not i.is_dir() and i.filename.startswith("sdcard/")
              and not i.filename.startswith("sdcard/Factory/")}
        for name, digest in old["files"].items():
            safe_name(name)
            require(sha(sd[name]) == digest and sha(z.read("sdcard/" + FACTORY + "/" + name)) == digest,
                    "Baseline resource mismatch: " + name)
    baseline_names = set(sd)
    local = {name.removeprefix(SD_REL + "/"): name for name in tracked if name.startswith(SD_REL + "/")}
    missing = baseline_names - set(local)
    require(missing == LARGE_ASSETS, "Unexpected baseline-only SD resources")
    previous_sd = dict(sd)
    previous_names = set(filter(None, git(repo, "ls-tree", "-r", "--name-only", PREVIOUS_COMMIT,
                                         "--", SD_REL + "/").decode().splitlines()))
    for name in previous_names:
        relative = name.removeprefix(SD_REL + "/")
        require(not relative.startswith("Factory/"), "Unexpected tracked previous Factory")
        previous_sd[relative] = git(repo, "show", PREVIOUS_COMMIT + ":" + name)
    previous_hashes = {name: sha(data) for name, data in previous_sd.items()}
    previous_hashes["Applications/Doom.app.elf"] = PREVIOUS_DOOM
    for relative, name in local.items():
        require(not relative.startswith("Factory/"), "Unexpected tracked Factory")
        sd[relative] = (repo / name).read_bytes()
    sd["Applications/Doom.app.elf"] = doom
    module = assignments(sd["MicroPython/lib/tab5factory.py"])
    require(module["VERSION"] == VERSION and len(module["FILES"]) == 180
            and set(module["FILES"]) == set(old["files"]), "Unexpected Factory names/version")
    upgraded = []
    for name in module["FILES"]:
        require(name in sd and name in previous_hashes, "Factory resource missing")
        current = sha(sd[name])
        for digest in {old["files"][name], previous_hashes[name]}:
            if current != digest:
                require(digest in module["PREVIOUS"].get(name, ()),
                        "Official update history missing: " + name)
        if current != previous_hashes[name]:
            upgraded.append(name)
    entries = {name: sha(sd[name]) for name in module["FILES"]}
    for name in module["FILES"]:
        sd[FACTORY + "/" + name] = sd[name]
    sd[FACTORY + "/manifest.json"] = json_bytes({"version": VERSION, "files": entries})
    apps = [n for n in sd if n.startswith("Applications/") and n.endswith(".app.elf")]
    require(len(apps) == 11, "Expected 11 native applications")
    for name in apps:
        raw = sd[name]
        require(raw[:6] == b"\x7fELF\x01\x01" and int.from_bytes(raw[18:20], "little") == 243,
                "Invalid native ELF: " + name)
    header = (repo / SKETCH_REL / "Tab5SystemResourcesData.h").read_text("utf8")
    arrays = {name: bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body))
              for name, body in re.findall(r"static const uint8_t (TAB5_RESOURCE_\d+)\[\] = \{(.*?)\};",
                                          header, re.S)}
    seeds = re.findall(r'\{"(/[^"\n]+)", (TAB5_RESOURCE_\d+), sizeof\(\2\), (\d+)\}', header)
    require(len(seeds) == 40, "Expected 40 embedded resources")
    for name, symbol, size in seeds:
        raw = zlib.decompress(arrays[symbol])
        require(len(raw) == int(size) and raw == sd[name[1:]], "Embedded resource differs: " + name)
    history = {"passed": True, "previousCommit": PREVIOUS_COMMIT, "previousDoomSha256": PREVIOUS_DOOM,
               "resourcesVerified": 180, "changedResources": sorted(upgraded),
               "method": "Previous Git SD overlay on the pinned baseline, with the previously delivered Doom ELF hash"}
    return sd, sorted(missing), history


def notices(repo):
    manifest = json.loads((repo / "firmware/LICENCES-manifest.json").read_bytes())
    require(manifest["version"] == VERSION, "Wrong notices version")
    entries = {}
    count = 0
    for component in manifest["components"]:
        for item in component["notices"]:
            path = safe_name(item["path"])
            raw = (repo / "firmware" / path).read_bytes()
            require(sha(raw) == item["sha256"], "Notice checksum differs: " + path)
            entries["notices/" + path] = raw
            count += 1
    require(count == 54, "Expected 54 verified notice entries")
    entries["LICENCES-manifest.json"] = (repo / "firmware/LICENCES-manifest.json").read_bytes()
    entries["LICENCES.md"] = (repo / "firmware/LICENCES.md").read_bytes()
    return entries


def checksums(entries):
    return ("\n".join(sha(raw.read_bytes() if isinstance(raw, Path) else raw) + "  " + name
                     for name, raw in sorted(entries.items())) + "\n").encode()


def write_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name, raw in sorted(entries.items()):
            safe_name(name)
            if isinstance(raw, Path):
                z.write(raw, name)
            else:
                z.writestr(name, raw)
    with zipfile.ZipFile(path) as z:
        check_archive(z)
        require(set(z.namelist()) == set(entries), "Nested SD ZIP member mismatch")
        for name, raw in entries.items():
            require(sha(z.read(name)) == sha(raw.read_bytes() if isinstance(raw, Path) else raw),
                    "Nested SD ZIP content mismatch")


def install_text():
    return f"""# Install this complete Tab5OS {VERSION} candidate

Build commit: {COMMIT}. The firmware still reports version {VERSION}; the commit
identifies this candidate. This archive has not replaced a public release.
Physical Tab5 validation remains pending.

## Contents

- downloads/Tab5OS_{VERSION}_OTA.bin: update for an existing installation.
- downloads/Tab5OS_{VERSION}_initial_install_16MB.bin: complete first-install image.
- downloads/Tab5OS_{VERSION}_SD.zip: complete SD card resources.
- {SD_REL}/: the same SD contents already extracted, including Factory.
- firmware/ and docs/: matching sources, build scripts, licenses and English guides.
- qa/: the exact successful CI build, sanitizer and native execution evidence.

## Update an existing installation while preserving personal data

1. Back up the card and customized scripts/settings. Copy the OTA BIN to the card,
   verify its SHA-256 against downloads/SHA256SUMS.txt, then use Explorer > Actions >
   Install firmware and restart.
2. Copy Factory/Tab5OS-{VERSION} from this complete SD package to the same path on
   your card, replacing that Factory reference as a complete set.
3. Copy MicroPython/update_sd.py and these MicroPython/lib/ helpers, backing up
   customized copies first: tab5factory.py, tab5workshop.py, tab5fs.py, tab5gui.py,
   tab5ui.py and tab5apps.py.
4. Restart, run /MicroPython/update_sd.py and choose **Repair and update**. It
   installs missing resources and updates recognized official resources, including
   Doom and language packs, with backups. Customized files are preserved.
5. Restart to load the updated resources. Compare any customized file reported by
   the updater with Factory before using the separate restore-modified action.

Keep personal System files, documents, games, ROMs, maps, Wikipedia archives,
vaults and saves. Do not replace the whole live System directory or clear the
card. Copying only firmware does not update the external ELF applications.

## First installation or new card

For a new FAT32 card, copy the CONTENTS of sdcard to its root: Applications,
MicroPython, System, Factory and the other supplied directories. Commercial
game ROMs and Wikipedia archives are supplied separately.

For first installation only, install Python and esptool 5, put the Tab5 in
download mode and run from the package root, replacing COM5 with your port:

    python -m pip install "esptool>=5,<6"
    python -m esptool --chip esp32p4 --port COM5 write-flash 0x0 downloads/Tab5OS_{VERSION}_initial_install_16MB.bin

The initial image replaces P4 Flash, including internal settings and keys.
Use OTA for an existing installation. Never write the OTA image at address 0x0.
The C6 firmware is unchanged. Keep power connected while writing.

## Validation scope

Doom title/gameplay checks execute this package's RV32 ELF with mocked OS
services. Separate sanitized host tests exercise the engine and startup failures.
Device startup diagnostics help investigate the reported tablet failure; these
computer tests do not establish that it is fixed on a physical tablet.

This remotely assembled package includes CI QA. The earlier local UI, theme and
localization evidence was unavailable to the remote packager and is not bundled
or represented as CI evidence. No browser screenshots are included.

SHA256SUMS.txt covers all package files except itself. downloads/SHA256SUMS.txt
covers the three install downloads. See docs/INSTALL_EN.md and docs/BUILD.md
for further usage and build instructions.
"""


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo", type=Path, required=True)
    p.add_argument("--artifact", type=Path, required=True)
    p.add_argument("--staging-dir", type=Path, required=True)
    args = p.parse_args()
    repo = args.repo.resolve(strict=True)
    require(sha(args.artifact.read_bytes()) == ARTIFACT_SHA, "Build artifact ZIP hash mismatch")
    tracked = verify_sources(repo)
    stage = args.staging_dir.resolve()
    require(not stage.exists() or (stage.is_dir() and not any(stage.iterdir())),
            "Staging directory must be absent or empty")
    require(stage != repo and not stage.is_relative_to(repo), "Staging must be outside source checkout")
    stage.mkdir(parents=True, exist_ok=True)
    package = stage / NAME
    package.mkdir()
    with zipfile.ZipFile(args.artifact) as artifact:
        check_archive(artifact)
        firmware_report, firmware, source_count = verify_firmware(artifact, repo)
        doom, doom_build = verify_doom(artifact, repo)
        execution = verify_runtime(artifact, repo, doom, doom_build)
        core = verify_core(artifact, repo)
        sd, missing, history = prepare_sd(repo, tracked, doom)
        sd_zip = package / ("downloads/Tab5OS_" + VERSION + "_SD.zip")
        sd_zip.parent.mkdir()
        sd_entries = {"sdcard/" + n: raw for n, raw in sd.items()}
        sd_entries.update(notices(repo))
        sd_entries["README.txt"] = (
            "Tab5OS complete candidate SD resources\n\n"
            "New card: copy the CONTENTS of sdcard to its root.\n"
            "Existing card: preserve personal System and data. Follow INSTALL_CANDIDATE.md\n"
            "in the complete package: copy Factory and updater helpers, then Repair and update.\n"
            "Includes 11 native apps, large fonts, IME and gazetteer resources.\n"
            "Commercial game ROMs and Wikipedia archives are supplied separately.\n").encode()
        sd_entries["SHA256SUMS.txt"] = checksums(sd_entries)
        write_zip(sd_zip, sd_entries)
        entries = {}
        for name in tracked:
            if not name.startswith(("downloads/", SD_REL + "/")):
                entries[name] = repo / name
        entries.update({SD_REL + "/" + n: raw for n, raw in sd.items()})
        downloads = {
            "downloads/Tab5OS_" + VERSION + "_OTA.bin": firmware[".bin"],
            "downloads/Tab5OS_" + VERSION + "_initial_install_16MB.bin": firmware[".merged.bin"],
            "downloads/Tab5OS_" + VERSION + "_SD.zip": sd_zip,
        }
        entries.update(downloads)
        entries["downloads/SHA256SUMS.txt"] = checksums({n.removeprefix("downloads/"): r
                                                       for n, r in downloads.items()})
        for item in artifact.infolist():
            if not item.is_dir() and item.filename.startswith(("qa/", "native/")):
                require(item.filename not in entries, "CI QA would overwrite source")
                entries[item.filename] = artifact.read(item)
        for suffix in (".elf", ".map", ".bootloader.bin", ".partitions.bin"):
            entries["qa/build/" + SKETCH + ".ino" + suffix] = firmware[suffix]
        for name in ("boot_app0.bin", "partitions.csv", "sdkconfig"):
            entries["qa/build/" + name] = artifact.read("build/" + name)
        entries["qa/package_candidate.py"] = Path(__file__).resolve()
        entries["qa/package-history-verification.json"] = json_bytes(history)
        entries["INSTALL_CANDIDATE.md"] = install_text().encode()
        entries["READ_FIRST.txt"] = (
            "Tab5OS complete candidate " + COMMIT[:7] + "\n\n"
            "Start with INSTALL_CANDIDATE.md. This includes firmware, the full SD resources,\n"
            "Factory, all 11 ELF apps, source, build scripts and English guides.\n"
            "Use OTA for an existing installation and preserve personal data.\n"
            "Physical tablet validation is pending.\n").encode()
        entries["README.md"] = (
            "# Complete candidate package\n\n"
            "Start with [candidate installation](INSTALL_CANDIDATE.md). This package includes "
            "compiled downloads and complete SD resources. [BUILD_INFO.json](BUILD_INFO.json) "
            "identifies the exact source and CI evidence.\n\n"
            "Doom title/gameplay tests run this ELF in RV32 emulation with mocked OS services. "
            "Device startup diagnostics are included; resolution of the reported physical-tablet "
            "startup failure remains unconfirmed. Only CI QA is bundled here; local host review "
            "files were unavailable to the remote packager.\n\n---\n\n").encode() + (repo / "README.md").read_bytes()
        info = {
            "status": "Compiled and automatically verified; physical device test pending",
            "version": VERSION, "validatedCommit": COMMIT, "validatedTree": TREE,
            "workflowRun": WORKFLOW, "artifactId": 13636557451, "artifactSha256": ARTIFACT_SHA,
            "firmwareSha256": sha(firmware[".bin"]), "firmwareBytes": len(firmware[".bin"]),
            "otaPartitionBytes": firmware_report["otaPartitionBytes"],
            "otaFreeBytes": firmware_report["otaFreeBytes"],
            "doomSha256": sha(doom), "doomBytes": len(doom), "nativeApplications": 11,
            "sdFilesIncludingFactory": len(sd), "factoryFiles": 180,
            "sourceFilesMatchedToBuild": source_count, "embeddedResourcesMatched": 40,
            "hostEvidenceFiles": 0,
            "localHostEvidence": "Not bundled: unavailable in the remote packaging environment",
            "doomExecutionReports": execution, "imageCoreVerification": core,
            "baselineSdSha256": BASE_SD_SHA, "unchangedLargeAssetsFromBaseline": missing,
            "previousCandidateUpdateVerified": history,
            "toolchain": firmware_report["toolchain"], "physicalDeviceTested": False,
            "publishedDownloadsReplaced": False, "packageFormat": "Complete directory tree uploaded as the final artifact ZIP",
        }
        entries["BUILD_INFO.json"] = json_bytes(info)
        entries["SHA256SUMS.txt"] = checksums(entries)
        require(len(entries) == len({n.casefold() for n in entries}), "Package case collision")
        for name, raw in entries.items():
            safe_name(name)
            out = package / name
            require(out.resolve().is_relative_to(package), "Output path escaped package")
            out.parent.mkdir(parents=True, exist_ok=True)
            if isinstance(raw, Path):
                if raw.resolve() != out.resolve():
                    shutil.copyfile(raw, out)
            else:
                out.write_bytes(raw)
            require(sha(out.read_bytes()) == sha(raw.read_bytes() if isinstance(raw, Path) else raw),
                    "Staged file checksum mismatch: " + name)
        actual = {p.relative_to(package).as_posix() for p in package.rglob("*") if p.is_file()}
        require(actual == set(entries), "Unexpected or missing staged package file")
        require(not git(repo, "status", "--porcelain", "--untracked-files=no").strip(),
                "Source changed during packaging")
        print(json.dumps({"passed": True, "stagingDir": str(stage), "packageRoot": NAME,
                          "files": len(entries), "bytes": sum(p.stat().st_size for p in package.rglob("*")
                                                             if p.is_file()), **info}, indent=2))


if __name__ == "__main__":
    main()
