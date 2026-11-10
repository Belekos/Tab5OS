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
COMMIT = None
TREE = None
PREVIOUS_COMMIT = "c08b0facb6db1c6bc136299444de13bbe81a4648"
PREVIOUS_PACKAGE_SHA = "6f306ef5657975038f68e9a968b9c6b150bcf856cc5e71c8bac62db1110dee33"
PREVIOUS_FACTORY_FILES = 186
ARTIFACT_SHA = None
BASE_SD_SHA = "d0a3d7987de2d1feb5aa91a1ee6ce1dc10170c0b453f63f7dd9438678e780463"
WORKFLOW = None
CONFIG = {}
WAD_SHA = "1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771"
SKETCH = "Tab5OS_Arduino_v" + VERSION
SKETCH_REL = "firmware/" + SKETCH
SD_REL = SKETCH_REL + "/sdcard"
FACTORY = "Factory/Tab5OS-" + VERSION
NAME = None
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
    require(len(payloads[".bin"]) == report["firmwareBytes"],
            "Unexpected firmware size")
    require(report["otaFreeBytes"] == report["otaPartitionBytes"] - len(payloads[".bin"]) > 0,
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


def verify_new_reports(z, repo):
    reports = {}
    for name in ("language-tests.json", "abi7-tests.json", "abi7-lvgl-mouse-tests.json",
                 "audio-driver-tests.json"):
        raw = z.read("qa/" + name)
        report = json.loads(raw)
        require(report.get("passed") is True, "Feature validation failed: " + name)
        for path, digest in report["sourceHashes"].items():
            require(sha((repo / safe_name(path)).read_bytes()) == digest,
                    "Feature validation source differs: " + path)
        require(report["sanitizers"] == ["AddressSanitizer", "UndefinedBehaviorSanitizer"],
                "Feature sanitizer coverage missing: " + name)
        if name == "language-tests.json":
            require(report["officialPacks"] == 14 and report["allocationFailureScenarios"] == 4,
                    "Language pack coverage differs")
            require(report["arduinoJsonVersion"] == "7.4.3", "Language parser dependency differs")
            for path, digest in report["packHashes"].items():
                require(sha((repo / SD_REL / "System/languages" / safe_name(path)).read_bytes()) == digest,
                        "Validated language pack differs: " + path)
        reports[name] = {"passed": True, "sha256": sha(raw)}
    dos = z.read("native/DOS.app.elf")
    require(dos[:6] == b"\x7fELF\x01\x01" and int.from_bytes(dos[18:20], "little") == 243
            and int.from_bytes(dos[16:18], "little") == 3, "Wrong DOS ELF target/type")
    for name in ("dos-folder-tests.json", "dos-overlay-tests.json", "dos-video-tests.json",
                 "dos-runtime-tests.json", "dos-runtime-rv32.json", "mouse-tests.json",
                 "dos-io-tests.json", "dos-app-report.json"):
        raw = z.read("qa/" + name)
        report = json.loads(raw)
        require(report.get("passed") is True, "DOS feature validation failed: " + name)
        for path, digest in report["sourceHashes"].items():
            require(sha((repo / safe_name(path)).read_bytes()) == digest,
                    "DOS feature validation source differs: " + path)
        if name == "dos-folder-tests.json":
            require(report.get("component") == "dos-folder" and
                    set(report.get("sanitizers", [])) ==
                    {"AddressSanitizer", "UndefinedBehaviorSanitizer", "LeakSanitizer"},
                    "DOS folder sanitizer validation is missing")
        elif name == "dos-overlay-tests.json":
            require(report.get("schema") == 1, "DOS overlay report schema differs")
            require({SKETCH_REL + "/tools/native-apps/dos/" + path for path in
                     ("overlay.c", "tests/test_overlay.c", "tests/run_overlay_tests.py")}
                    <= set(report["sourceHashes"]), "DOS overlay source manifest is incomplete")
        elif name == "dos-video-tests.json":
            require(report.get("schema") == 1 and
                    all(report.get("checks", {}).get(key) is True for key in
                        ("same_pixels_across_six_bios_modes_and_blink", "downscale_matches_original",
                         "synchronous_skips_vram_copy")), "DOS video equivalence coverage failed")
        elif name == "dos-runtime-tests.json":
            require(report.get("component") == "dos-runtime-primitives" and
                    set(report.get("sanitizers", [])) ==
                    {"AddressSanitizer", "UndefinedBehaviorSanitizer", "LeakSanitizer"},
                    "DOS memory/arithmetic sanitizer coverage missing")
        elif name == "dos-runtime-rv32.json":
            require(report.get("component") == "dos-runtime-rv32-instructions" and
                    report.get("baselineCommit") == PREVIOUS_COMMIT and report.get("rows"),
                    "DOS target runtime comparison is missing or uses another baseline")
        elif name == "dos-io-tests.json":
            require(report.get("referenceDispatchEquivalent") is True and
                    report.get("checks", 0) >= 655368 and report.get("aliasCount") == 64,
                    "DOS video-port dispatch equivalence coverage missing")
        elif name == "mouse-tests.json":
            tests = report.get("tests", [])
            require(len(tests) == 2 and all(case.get("passed") is True for case in tests),
                    "DOS mouse frontend/guest driver validation is incomplete")
            require(report.get("imageSha256") == sha((repo / SD_REL / "DOS/FREEDOS.IMG").read_bytes())
                    and report.get("driversSha256") ==
                    sha((repo / SD_REL / "System/DOS/DRIVERS.IMG").read_bytes()),
                    "DOS mouse tests used different boot/driver images")
        else:
            require(report.get("sourcesUnchanged") is True and report.get("inputsUnchanged") is True,
                    "DOS execution changed its sources or inputs")
            require(report["elf"] == {"sha256": sha(dos), "bytes": len(dos)},
                    "DOS execution used a different ELF")
            image = (repo / SD_REL / "DOS/FREEDOS.IMG").read_bytes()
            require(report["image"] == {"sha256": sha(image), "bytes": len(image)},
                    "DOS execution used a different boot image")
            drivers = (repo / SD_REL / "System/DOS/DRIVERS.IMG").read_bytes()
            require(report["drivers"] == {"sha256": sha(drivers), "bytes": len(drivers)},
                    "DOS execution used a different mouse driver image")
            scenarios = report.get("scenarios", [])
            require(len(scenarios) == 3 and {case.get("scenario") for case in scenarios} ==
                    {"allocation-failure", "missing-boot", "launch"} and
                    all(case.get("passed") is True for case in scenarios),
                    "DOS frontend validation coverage is incomplete")
            launch = next(case for case in scenarios if case["scenario"] == "launch")
            require(all(launch.get("checks", {}).get(key) is True for key in
                        ("driver_disk_opened", "mouse_motion_left_right", "audible_pcm_mute_restore")),
                    "DOS mouse/audio frontend validation failed")
        reports[name] = {"passed": True, "sha256": sha(raw)}
    raw = z.read("qa/dos-audio-tests.json")
    report = json.loads(raw)
    require(report.get("passed") is True and report.get("sample_rate") == 22050 and
            report.get("core_checks", 0) >= 168226 and report.get("opl_pcm_hash") == "91f11fc9"
            and report.get("audio_output_passed") is True
            and "OPL NATIVE EQUIVALENCE PASS" in report.get("core_stdout", ""),
            "DOS audio core/output/equivalence regression coverage failed")
    require({"app_audio.h", "tests/run_audio_tests.py", "tests/audio_host.c",
             "tests/audio_output_host.c"} <= set(report.get("source_sha256", {})),
            "DOS audio source manifest is incomplete")
    for path, digest in report["source_sha256"].items():
        require(sha((repo / SKETCH_REL / "tools/native-apps/dos" / safe_name(path)).read_bytes()) == digest,
                "DOS audio validation source differs: " + path)
    reports["dos-audio-tests.json"] = {"passed": True, "sha256": sha(raw)}
    return dos, reports


def prepare_sd(repo, tracked, doom, dos, previous_package):
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
    require(sha(previous_package.read_bytes()) == PREVIOUS_PACKAGE_SHA, "Wrong previous complete package")
    with zipfile.ZipFile(previous_package) as previous:
        check_archive(previous)
        prefix = "Tab5OS_0.8.53_c08b0fa_complete/"
        info = json.loads(previous.read(prefix + "BUILD_INFO.json"))
        require(info["validatedCommit"] == PREVIOUS_COMMIT, "Wrong previous candidate source")
        previous_factory = json.loads(previous.read(prefix + SD_REL + "/" + FACTORY + "/manifest.json"))
        previous_hashes = previous_factory["files"]
        require(len(previous_hashes) == PREVIOUS_FACTORY_FILES and set(old["files"]) <= set(previous_hashes),
                "Unexpected previous managed resources")
        for name, digest in previous_hashes.items():
            for path in (name, FACTORY + "/" + name):
                require(sha(previous.read(prefix + SD_REL + "/" + safe_name(path))) == digest,
                        "Previous Factory/live mismatch: " + name)
        previous_dos_prefix = prefix + SD_REL + "/DOS/"
        previous_dos = {"DOS/" + item.filename[len(previous_dos_prefix):]: previous.read(item)
                        for item in previous.infolist()
                        if not item.is_dir() and item.filename.startswith(previous_dos_prefix)}
        require(len(previous_dos) == 5 and "DOS/FREEDOS.IMG" in previous_dos,
                "Previous DOS session resources are incomplete")
    for relative, name in local.items():
        require(not relative.startswith("Factory/"), "Unexpected tracked Factory")
        sd[relative] = (repo / name).read_bytes()
    sd["Applications/Doom.app.elf"] = doom
    sd["Applications/DOS.app.elf"] = dos
    # c08 snapshots identify both the boot bytes and the mounted folder layout.
    # Keep the default /DOS mount unchanged too; new drivers belong in /System/DOS.
    require({name for name in sd if name.startswith("DOS/")} == set(previous_dos),
            "The DOS mount layout changed; this would hide existing prompt snapshots")
    for name, raw in previous_dos.items():
        require(sd[name] == raw, "DOS session base changed from the delivered candidate: " + name)
    module = assignments(sd["MicroPython/lib/tab5factory.py"])
    require(module["VERSION"] == VERSION and len(module["FILES"]) == CONFIG["factoryFiles"]
            and set(previous_hashes) <= set(module["FILES"]), "Unexpected Factory names/version")
    upgraded = []
    for name in module["FILES"]:
        require(name in sd, "Factory resource missing")
        current = sha(sd[name])
        for digest in {old["files"].get(name), previous_hashes.get(name)} - {None}:
            if current != digest:
                require(digest in module["PREVIOUS"].get(name, ()),
                        "Official update history missing: " + name)
        if current != previous_hashes.get(name):
            upgraded.append(name)
    entries = {name: sha(sd[name]) for name in module["FILES"]}
    for name in module["FILES"]:
        sd[FACTORY + "/" + name] = sd[name]
    sd[FACTORY + "/manifest.json"] = json_bytes({"version": VERSION, "files": entries})
    apps = [n for n in sd if n.startswith("Applications/") and n.endswith(".app.elf")]
    require(len(apps) == CONFIG["nativeApplications"] == 12, "Expected 12 native applications")
    for name in apps:
        raw = sd[name]
        require(raw[:6] == b"\x7fELF\x01\x01" and int.from_bytes(raw[18:20], "little") == 243,
                "Invalid native ELF: " + name)
    header = (repo / SKETCH_REL / "Tab5SystemResourcesData.h").read_text("utf8")
    arrays = {name: bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body))
              for name, body in re.findall(r"static const uint8_t (TAB5_RESOURCE_\d+)\[\] = \{(.*?)\};",
                                          header, re.S)}
    seeds = re.findall(r'\{"(/[^"\n]+)", (TAB5_RESOURCE_\d+), sizeof\(\2\), (\d+)\}', header)
    require(len(seeds) == CONFIG["embeddedResources"] >= 40, "Unexpected embedded resources")
    for name, symbol, size in seeds:
        raw = zlib.decompress(arrays[symbol])
        require(len(raw) == int(size) and raw == sd[name[1:]], "Embedded resource differs: " + name)
    history = {"passed": True, "previousCommit": PREVIOUS_COMMIT, "previousPackageSha256": PREVIOUS_PACKAGE_SHA,
               "resourcesVerified": len(module["FILES"]), "changedResources": sorted(upgraded),
               "dosSessionBasePreserved": {name: sha(raw) for name, raw in sorted(previous_dos.items())},
               "method": "Every previous delivered live and Factory resource checked in the exact complete ZIP"}
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
    require(count >= 54, "Missing existing notice entries")
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
   Doom, DOS and language packs, with backups. Customized files are preserved.
5. Restart to load the updated resources. Compare any customized file reported by
   the updater with Factory before using the separate restore-modified action.

Keep personal System files, documents, games, ROMs, maps, Wikipedia archives,
vaults and saves. Do not replace the whole live System directory or clear the
card. Copying only firmware does not update the external ELF applications.

## Existing DOS disk snapshots

The FreeDOS boot image and existing /DOS resources are byte-for-byte identical
to candidate c08b0fa. Additional driver resources live in /System/DOS, outside
the default mounted folder. This preserves the boot and default-folder identity
used by its saved floppy sectors. The update does not replace /Saves/DOS files.
Sessions launched from other program folders retain their identity when their
paths and directory layout are unchanged. Moving a program or changing that
layout can select a different snapshot; it does not erase the earlier file.

For the current DOS controls and capabilities, see {SKETCH_REL}/DOS.md.

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
    global COMMIT, TREE, ARTIFACT_SHA, WORKFLOW, NAME, CONFIG
    p.add_argument("--config", type=Path, required=True)
    p.add_argument("--previous-package", type=Path, required=True)
    p.add_argument("--repo", type=Path, required=True)
    p.add_argument("--artifact", type=Path, required=True)
    p.add_argument("--staging-dir", type=Path, required=True)
    args = p.parse_args()
    CONFIG = json.loads(args.config.read_bytes())
    COMMIT, TREE = CONFIG["commit"], CONFIG["tree"]
    ARTIFACT_SHA, WORKFLOW = CONFIG["artifactSha256"], CONFIG["workflowRun"]
    require(re.fullmatch(r"[0-9a-f]{40}", COMMIT) and re.fullmatch(r"[0-9a-f]{40}", TREE), "Invalid source identity")
    require(re.fullmatch(r"[0-9a-f]{64}", ARTIFACT_SHA), "Invalid artifact digest")
    require(re.fullmatch(r"https://github\.com/Belekos/Tab5OS/actions/runs/[0-9]+", WORKFLOW), "Invalid build URL")
    NAME = "Tab5OS_" + VERSION + "_" + COMMIT[:7] + "_complete"
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
        dos, new_reports = verify_new_reports(artifact, repo)
        sd, missing, history = prepare_sd(repo, tracked, doom, dos, args.previous_package)
        sd_zip = package / ("downloads/Tab5OS_" + VERSION + "_SD.zip")
        sd_zip.parent.mkdir()
        sd_entries = {"sdcard/" + n: raw for n, raw in sd.items()}
        sd_entries.update(notices(repo))
        sd_entries["README.txt"] = (
            "Tab5OS complete candidate SD resources\n\n"
            "New card: copy the CONTENTS of sdcard to its root.\n"
            "Existing card: preserve personal System and data. Follow INSTALL_CANDIDATE.md\n"
            "in the complete package: copy Factory and updater helpers, then Repair and update.\n"
            "Includes 12 native apps, large fonts, IME and gazetteer resources.\n"
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
        entries["qa/package-config.json"] = json_bytes(CONFIG)
        entries["qa/package-history-verification.json"] = json_bytes(history)
        entries["INSTALL_CANDIDATE.md"] = install_text().encode()
        entries["READ_FIRST.txt"] = (
            "Tab5OS complete candidate " + COMMIT[:7] + "\n\n"
            "Start with INSTALL_CANDIDATE.md. This includes firmware, the full SD resources,\n"
            "Factory, all 12 ELF apps, source, build scripts and English guides.\n"
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
            "workflowRun": WORKFLOW, "artifactId": CONFIG["artifactId"], "artifactSha256": ARTIFACT_SHA,
            "firmwareSha256": sha(firmware[".bin"]), "firmwareBytes": len(firmware[".bin"]),
            "otaPartitionBytes": firmware_report["otaPartitionBytes"],
            "otaFreeBytes": firmware_report["otaFreeBytes"],
            "doomSha256": sha(doom), "doomBytes": len(doom), "nativeApplications": 12,
            "dosSha256": sha(dos), "dosBytes": len(dos), "newFeatureReports": new_reports,
            "sdFilesIncludingFactory": len(sd), "factoryFiles": CONFIG["factoryFiles"],
            "sourceFilesMatchedToBuild": source_count, "embeddedResourcesMatched": CONFIG["embeddedResources"],
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

