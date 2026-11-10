"""Independently audit a pinned compiled candidate and exact previous delivery."""
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

PREVIOUS_SHA = "6f306ef5657975038f68e9a968b9c6b150bcf856cc5e71c8bac62db1110dee33"
PREVIOUS_PREFIX = "Tab5OS_0.8.53_c08b0fa_complete/"
PREVIOUS_COMMIT = "c08b0facb6db1c6bc136299444de13bbe81a4648"
PREVIOUS_FACTORY_FILES = 186
BASE_SHA = "d0a3d7987de2d1feb5aa91a1ee6ce1dc10170c0b453f63f7dd9438678e780463"
VERSION = "0.8.53"
SKETCH = "firmware/Tab5OS_Arduino_v0.8.53/"
SD = SKETCH + "sdcard/"
FACTORY = "Factory/Tab5OS-0.8.53/"
LARGE = {
    "Games/GB/Tab5_Pocket.gb", "Games/GBC/Tab5_Color.gbc",
    "System/fonts/cjk-ja.ttf", "System/fonts/cjk-ko.ttf", "System/fonts/cjk-zh.ttf",
    "System/ime/ja-kana.dat", "System/ime/sources/SKK-JISYO.L.gz",
    "System/ime/zh-pinyin.dat", "System/maps/gazetteer/cities.t5geo",
}
APPS = {"DOS", "Doom", "GBA", "Maze3D", "MegaDrive", "NES", "Pentest", "SNES",
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


def verify_host_reports(read):
    language = json.loads(read("qa/language-tests.json"))
    require(language.get("passed") is True and language.get("officialPacks") == 14,
            "Production language loader did not accept all 14 official packs")
    require(language.get("checks", 0) >= 50000 and
            language.get("allocationFailureScenarios", 0) >= 4 and
            language.get("maxLanguageFileBytes") == 1024 * 1024,
            "Production language loader regression coverage differs")
    require(language.get("arduinoJsonVersion") == "7.4.3", "Language metadata dependency differs")
    require(set(language.get("sanitizers", [])) == {"AddressSanitizer", "UndefinedBehaviorSanitizer"},
            "Language CI sanitizers missing")
    required_language_sources = {SKETCH + name for name in (
        "Tab5Localization.cpp", "Tab5LanguageReader.h", "Tab5LanguageMetadata.h",
        "Tab5TranslationFormat.h", "Tab5EnglishFallback.h", "Tab5EnglishBitmap.h",
        "tools/tests/languages/test_languages.cpp", "tools/tests/languages/run_tests.py")}
    require(required_language_sources <= set(language.get("sourceHashes", {})),
            "Language production/test source manifest is incomplete")
    for name, digest in language["sourceHashes"].items():
        safe(name)
        require(sha(read(name)) == digest, "Language test source differs: " + name)
    packs = {code + ".json" for code in (
        "ar", "de-DE", "en-US", "es-ES", "fr-FR", "id-ID", "it-IT", "ja-JP",
        "ko-KR", "nl-NL", "pt-BR", "pt-PT", "ru-RU", "zh-CN")}
    require(set(language.get("packHashes", {})) == packs, "Language pack test inventory differs")
    for name, digest in language["packHashes"].items():
        require(sha(read(SD + "System/languages/" + name)) == digest,
                "Packaged language was not tested: " + name)
    abi = json.loads(read("qa/abi7-tests.json"))
    require(abi.get("component") == "native-abi7" and abi.get("passed") is True, "ABI tests failed")
    require(set(abi.get("sanitizers", [])) == {"AddressSanitizer", "UndefinedBehaviorSanitizer"},
            "ABI CI sanitizers missing")
    tests = abi.get("tests", [])
    require(len(tests) == 6 and {t.get("test") for t in tests} ==
            {"test_keyboard_directory", "test_broker", "test_native_keyboard",
             "test_native_mouse", "test_mouse_capture", "test_native_runtime"} and
            all(t.get("result") == "passed" for t in tests),
            "ABI7 keyboard/directory/mouse/capture tests failed")
    abi_hashes = {name.replace("\\", "/"): digest for name, digest in abi.get("sourceHashes", {}).items()}
    require(len(abi_hashes) == len(abi.get("sourceHashes", {})), "Duplicate ABI source path")
    require({SKETCH + name for name in (
             "Tab5Executable.cpp", "Tab5NativeView.cpp", "WriterUsb.cpp", "Tab5AppApi.h",
             "Tab5AppDirectory.h", "Tab5KeyboardState.h", "Tab5Mouse.cpp", "Tab5Mouse.h",
             "WriterMouse.h", "Tab5AppInputQueue.h", "tools/tests/abi7/test_broker.cpp",
             "tools/tests/abi7/test_keyboard_directory.cpp", "tools/tests/abi7/test_native_keyboard.cpp",
             "tools/tests/abi7/test_native_mouse.cpp", "tools/tests/abi7/test_mouse_capture.cpp",
             "tools/tests/abi7/test_native_runtime.cpp")}
            <= set(abi_hashes), "ABI production/test source manifest is incomplete")
    for name, digest in abi_hashes.items():
        safe(name)
        require(sha(read(name)) == digest, "ABI test source differs: " + name)
    return {"languageChecks": language["checks"], "languagePacks": len(packs), "abi7Tests": len(tests),
            "languageReportSha256": sha(read("qa/language-tests.json")),
            "abi7ReportSha256": sha(read("qa/abi7-tests.json")),
            "nativeIo": verify_native_io(read)}


def verify_native_io(read):
    result = {}
    for name, required in (
            ("abi7-lvgl-mouse-tests.json", {SKETCH + path for path in (
                "Tab5Mouse.cpp", "Tab5Mouse.h", "WriterMouse.h",
                "tools/tests/abi7/test_mouse_capture_lvgl.cpp",
                "tools/tests/abi7/run_mouse_capture_lvgl.py")}),
            ("audio-driver-tests.json", {SKETCH + path for path in (
                "Tab5AudioDriver.cpp", "Tab5AudioDriver.h",
                "tools/tests/audio-driver/test_audio_driver.cpp",
                "tools/tests/audio-driver/run_tests.py")})):
        raw = read("qa/" + name)
        report = json.loads(raw)
        require(report.get("passed") is True, "Native I/O regression failed: " + name)
        require(required <= set(report.get("sourceHashes", {})),
                "Native I/O source manifest incomplete: " + name)
        for source, digest in report["sourceHashes"].items():
            safe(source)
            require(sha(read(source)) == digest, "Native I/O test source differs: " + source)
        if name == "abi7-lvgl-mouse-tests.json":
            require(report.get("component") == "native-mouse-lvgl" and
                    report.get("lvglVersion") == "9.6.0", "Mouse test LVGL dependency differs")
            tests = report.get("tests", [])
            require(len(tests) == 4 and {case.get("test") for case in tests} ==
                    {"touch", "mouse", "held-release", "disconnect"} and
                    all(case.get("result") == "passed" for case in tests),
                    "Real LVGL capture/release/disconnect coverage missing")
            guards = report.get("regressionGuards", [])
            require(len(guards) == 2 and {case.get("test") for case in guards} == {"touch", "mouse"}
                    and all(case.get("result") == "detected" and case.get("exitCode") == 2 for case in guards),
                    "Mouse regression tests did not detect synchronous input re-entry")
            require(set(report.get("sanitizers", [])) == {"AddressSanitizer", "UndefinedBehaviorSanitizer"},
                    "Real LVGL input sanitizer coverage missing")
            hashes = report.get("lvglSourceHashes", {})
            require(len(hashes) > 10 and all(re.fullmatch(r"[0-9a-f]{64}", h) for h in hashes.values()),
                    "Real LVGL source fingerprints missing")
        else:
            require(report.get("component") == "Common PCM driver buffering and lifetime" and
                    report.get("checks", 0) >= 45 and
                    set(report.get("sanitizers", [])) == {"AddressSanitizer", "UndefinedBehaviorSanitizer"},
                    "PCM buffering/lifetime sanitizer coverage missing")
        result[name] = {"passed": True, "sha256": sha(raw),
                        "sourceFilesVerified": len(report["sourceHashes"])}
    return result


def verify_dos_boot(read):
    prefix = SKETCH + "tools/native-apps/dos/"
    provenance = json.loads(read(prefix + "licenses/freedos/provenance.json"))
    image = read(SD + "DOS/FREEDOS.IMG")
    require(len(image) == provenance["image"]["size"] == 1474560 and
            sha(image) == provenance["image"]["sha256"], "FreeDOS boot image provenance differs")
    require(provenance.get("optional_keyboard_tools") is True, "Guest keyboard layout tools missing")
    archive_names = {"freecom-source.zip": "freecom-source.zip",
                     "kernel-source.zip": "kernel-source.zip",
                     "keyb-source.zip": "keyb-source.zip",
                     "keyb-layouts-3.1a.zip": "KEYB_LAY.ZIP"}
    for name, key in archive_names.items():
        raw = read(prefix + "licenses/freedos/" + name)
        require(sha(raw) == provenance["extracted_release_objects_sha256"][key],
                "FreeDOS matching source archive differs: " + name)
    report = json.loads(read("qa/dos-boot-report.json"))
    require(report.get("schema") == 1 and report.get("passed") is True and
            report.get("returncode") == 0, "Real FreeDOS boot/executable test failed")
    tested_image = report["freedos_image"]
    require(tested_image["sha256"] == sha(image) and tested_image["bytes"] == len(image) and
            tested_image["kernel"] == "2043" and tested_image["shell"] == "0.86a",
            "DOS runtime test used a different OS image")
    markers = {"FreeDOS kernel 2043", "FreeCom version 0.86", "TAB5OS DOS READY",
               "TAB5 COM OK", "TAB5 MZ EXE OK", "TAB5 ADJACENT DATA OK", "HELLOL~1.EXE"}
    required_checks = markers | {"base_image_unchanged"}
    require(required_checks <= set(report.get("checks", {})) and
            all(report["checks"][name] is True for name in required_checks),
            "DOS COM/MZ, adjacent-data, long-path or base-image checks failed")
    output = report.get("stdout", "") + "\n" + report.get("stderr", "")
    require(all(marker in output for marker in markers), "DOS guest output markers missing")
    source_hashes = report.get("sourceHashes", {})
    required_sources = {prefix + name for name in (
        "core/x86/x86.c", "core/ibmpc/pc.c", "core/diskspace/fat.c", "tests/boot_host.c",
        "tests/build_host.py", "tests/test_boot.py", "tests/make_examples.py")}
    require(required_sources <= set(source_hashes), "DOS host runtime source manifest is incomplete")
    for name, digest in source_hashes.items():
        safe(name)
        require(sha(read(name)) == digest, "DOS host test source differs: " + name)
    require(set(report.get("fixtureHashes", {})) == {"HELLO.COM", "HELLO.EXE", "DATA.TXT"},
            "DOS guest fixture inventory differs")
    for name, digest in report["fixtureHashes"].items():
        require(sha(read(SD + "DOS/Examples/" + name)) == digest,
                "Distributed DOS example was not guest-tested: " + name)
    drivers = read(SD + "System/DOS/DRIVERS.IMG")
    mouse = json.loads(read(prefix + "licenses/cutemouse/provenance.json"))
    require(mouse["image"]["size"] == len(drivers) == 1474560 and
            mouse["image"]["sha256"] == sha(drivers), "Mouse driver image provenance differs")
    require(sha(read(prefix + "licenses/cutemouse/ctmouse-source.zip")) ==
            mouse["expected"]["ctmouse-source.zip"], "Mouse driver corresponding sources differ")
    return {"passed": True, "freedosImageSha256": sha(image), "checks": len(required_checks),
            "driversImageSha256": sha(drivers),
            "reportSha256": sha(read("qa/dos-boot-report.json")),
            "sourceFilesVerified": len(source_hashes)}


def verify_dos_native(read):
    elf = read("native/DOS.app.elf")
    require(read(SD + "Applications/DOS.app.elf") == elf, "Packaged DOS differs from CI")
    require(elf[:6] == b"\x7fELF\x01\x01" and int.from_bytes(elf[16:18], "little") == 3 and
            int.from_bytes(elf[18:20], "little") == 243, "Invalid DOS RISC-V ELF32 DYN binary")
    report = json.loads(read("qa/native-admission-report.json"))
    require(report.get("passed") is True and report.get("sourcesUnchanged") is True,
            "Native ELF admission failed or modified sources")
    artifacts = report.get("artifacts", [])
    require(artifacts and all(a.get("passed") is True and a.get("artifactUnchanged") is True
                              for a in artifacts), "Native admission modified/failed an artifact")
    dos = [a for a in artifacts if a.get("artifact", "").replace("\\", "/").rsplit("/", 1)[-1]
           == "DOS.app.elf"]
    require(len(dos) == 1, "DOS admission evidence missing or duplicated")
    admission = dos[0]
    require(admission["sha256"] == sha(elf) and admission["bytes"] == len(elf),
            "DOS ELF differs from admitted binary")
    require(admission["imports"] == ["tab5_app_api_v7"] and
            admission["expectedImport"] == "tab5_app_api_v7", "DOS does not use ABI7")
    image = admission["image"]
    require(0 < image["imageBytes"] <= 8 * 1024 * 1024 and
            16384 <= image["stackBytes"] <= 256 * 1024, "DOS image/stack violates loader bounds")
    hashes = report.get("sourceHashes", {})
    require(len(hashes) >= 10, "Native admission source manifest is incomplete")
    for name, digest in hashes.items():
        safe(name)
        require(sha(read(name)) == digest, "Native admission source differs: " + name)
    return {"passed": True, "elfSha256": sha(elf), "elfBytes": len(elf),
            "reportSha256": sha(read("qa/native-admission-report.json")),
            "sourceFilesVerified": len(hashes), "imports": admission["imports"]}


def verify_dos_execution(read):
    prefix = SKETCH + "tools/native-apps/dos/"
    folder = json.loads(read("qa/dos-folder-tests.json"))
    require(folder.get("passed") is True and folder.get("component") == "dos-folder",
            "DOS folder projection tests failed")
    require(set(folder.get("sanitizers", [])) ==
            {"AddressSanitizer", "UndefinedBehaviorSanitizer", "LeakSanitizer"},
            "DOS folder sanitizer coverage missing")
    required_folder_sources = {prefix + path for path in
                               ("platform.c", "core/ibmpc/folder.c", "tests/test_folder.c",
                                "tests/run_folder_tests.py")}
    require(required_folder_sources <= set(folder.get("sourceHashes", {})),
            "DOS folder production/test source manifest is incomplete")
    for name, digest in folder["sourceHashes"].items():
        safe(name)
        require(sha(read(name)) == digest, "DOS folder source differs: " + name)

    overlay = json.loads(read("qa/dos-overlay-tests.json"))
    require(overlay.get("schema") == 1 and overlay.get("passed") is True,
            "DOS overlay save/reload regression tests failed")
    require({prefix + path for path in ("overlay.c", "platform.h", "tests/test_overlay.c",
                                       "tests/run_overlay_tests.py", "tests/host-compat/platform.h")}
            <= set(overlay.get("sourceHashes", {})), "DOS overlay source manifest is incomplete")
    for name, digest in overlay["sourceHashes"].items():
        safe(name)
        require(sha(read(name)) == digest, "DOS overlay source differs: " + name)

    report = json.loads(read("qa/dos-app-report.json"))
    require(report.get("schemaVersion") == 1 and report.get("passed") is True and
            report.get("component") == "Exact DOS RV32 frontend" and
            report.get("sourcesUnchanged") is True and report.get("inputsUnchanged") is True,
            "Exact DOS frontend execution failed or changed sources/inputs")
    for name, raw in (("elf", read("native/DOS.app.elf")), ("image", read(SD + "DOS/FREEDOS.IMG")),
                      ("drivers", read(SD + "System/DOS/DRIVERS.IMG"))):
        require(report[name] == {"bytes": len(raw), "sha256": sha(raw)},
                "Exact DOS execution used a different " + name)
    required_sources = {prefix + path for path in
                        ("app.c", "platform.c", "core/x86/x86.c", "core/ibmpc/pc.c",
                         "app_mouse.h", "app_audio.h", "tests/peripheral_fixture.py",
                         "tests/emulate_app.py", "tests/frame_text.py", "tests/make_examples.py")}
    require(required_sources <= set(report.get("sourceHashes", {})),
            "DOS frontend execution source manifest is incomplete")
    for name, digest in report["sourceHashes"].items():
        safe(name)
        require(sha(read(name)) == digest, "DOS frontend source differs: " + name)
    fixtures = {"HELLO.COM": "HELLO.COM", "Hello long name.EXE": "HELLO.EXE", "DATA.TXT": "DATA.TXT"}
    require(set(report.get("fixtureHashes", {})) == set(fixtures) | {"MOUSE.COM"},
            "DOS frontend fixture inventory differs")
    require(re.fullmatch(r"[0-9a-f]{64}", report["fixtureHashes"]["MOUSE.COM"]),
            "DOS peripheral fixture identity is missing")
    for fixture, packaged in fixtures.items():
        require(report["fixtureHashes"][fixture] == sha(read(SD + "DOS/Examples/" + packaged)),
                "DOS frontend fixture differs from the distributed example: " + fixture)
    expected = {"allocation-failure": -1, "missing-boot": -1, "launch": 0}
    scenarios = report.get("scenarios", [])
    require(len(scenarios) == len(expected) and {case.get("scenario") for case in scenarios} == set(expected),
            "DOS frontend scenario set differs")
    common_checks = {"abi7_only", "returned_to_os", "no_callback_or_memory_error",
                     "base_files_unchanged", "heap_within_budget", "file_handles_within_budget"}
    launch_checks = {"return_code_zero", "boot_ready_logged", "rendered_boot_com_mz_data",
                     "physical_keyboard_completed", "queued_text_did_not_duplicate_input", "pointer_exit",
                     "adjacent_data_opened", "selected_directory_enumerated", "files_closed_on_exit",
                     "canvas_started", "driver_disk_opened", "mouse_motion_left_right",
                     "audible_pcm_mute_restore"}
    for case in scenarios:
        require(case.get("passed") is True and case.get("stopReason") == "application-returned" and
                case.get("returnCode") == expected[case["scenario"]] and case.get("error") is None,
                "DOS frontend scenario failed: " + case["scenario"])
        required = common_checks | (launch_checks if case["scenario"] == "launch" else set())
        require(required <= set(case.get("checks", {})) and
                all(case["checks"][name] is True for name in required),
                "DOS frontend scenario coverage is incomplete")
        require(case["loader"]["imports"] == ["tab5_app_api_v7"], "DOS execution bypassed ABI7")
        if case["scenario"] == "launch":
            require({"TAB5OS DOS READY", "TAB5 MZ EXE OK", "TAB5 ADJACENT DATA OK", "TAB5 COM OK",
                     "TAB5 MOUSE READY", "TAB5 MOUSE MOVE OK", "TAB5 MOUSE LEFT OK",
                     "TAB5 MOUSE RIGHT OK", "TAB5 AUDIO READY", "TAB5 INPUT AUDIO OK"}
                    <= set(case.get("renderedMarkers", {})), "DOS rendered guest output is missing")
            require(case.get("openHandlesAtReturn") == 0, "DOS frontend leaked file handles")
            require(len(case.get("mouseSteps", [])) == 3 and case.get("volumeTaps") == [1, 2],
                    "DOS peripheral interaction sequence is incomplete")
            require(all(case.get("audioProbe", {}).get(key) is True for key in ("nonzero", "muted", "resumed")),
                    "DOS audio mute/restore did not complete")
    return {"passed": True, "scenarios": len(scenarios),
            "folderReportSha256": sha(read("qa/dos-folder-tests.json")),
            "overlayReportSha256": sha(read("qa/dos-overlay-tests.json")),
            "executionReportSha256": sha(read("qa/dos-app-report.json")),
            "peripheralFixtureSha256": report["fixtureHashes"]["MOUSE.COM"],
            "executionSourcesVerified": len(report["sourceHashes"])}


def verify_dos_optimizations(read):
    prefix = SKETCH + "tools/native-apps/dos/"
    runtime_source = SKETCH + "tools/native-apps/common/runtime.c"
    result = {}
    for name, required in (
            ("dos-video-tests.json", {prefix + path for path in (
                "presentation.h", "core/ibmpc/video.c", "tests/test_video.c",
                "tests/test_presentation.c", "pacing.h", "tests/test_pacing.c",
                "tests/run_video_tests.py")}),
            ("dos-runtime-tests.json", {runtime_source, prefix + "tests/test_runtime_primitives.c",
                                        prefix + "tests/run_runtime_tests.py"}),
            ("dos-io-tests.json", {prefix + path for path in (
                "core/ibmpc/pc.c", "tests/test_io_dispatch.c", "tests/run_io_tests.py")}),
            ("dos-runtime-rv32.json", {runtime_source, prefix + "core/ibmpc/pit.c",
                                      prefix + "tests/measure_runtime_rv32.py"})):
        raw = read("qa/" + name)
        report = json.loads(raw)
        require(report.get("passed") is True, "DOS optimization verification failed: " + name)
        require(required <= set(report.get("sourceHashes", {})),
                "DOS optimization source manifest incomplete: " + name)
        for source, expected in report["sourceHashes"].items():
            safe(source)
            require(sha(read(source)) == expected, "DOS optimization source differs: " + source)
        if name == "dos-video-tests.json":
            require(report.get("schema") == 1 and
                    all(report.get("checks", {}).get(key) is True for key in (
                        "same_pixels_across_six_bios_modes_and_blink", "downscale_matches_original",
                        "synchronous_skips_vram_copy", "bounded_fractional_frame_pacing",
                        "staticGraphicsStable", "textBlinkAndCursor", "rasterPaletteAndMode")),
                    "DOS presentation or bounded frame pacing verification failed")
        elif name == "dos-runtime-tests.json":
            require(report.get("component") == "dos-runtime-primitives" and
                    set(report.get("sanitizers", [])) ==
                    {"AddressSanitizer", "UndefinedBehaviorSanitizer", "LeakSanitizer"},
                    "DOS runtime memory/arithmetic sanitizer coverage missing")
        elif name == "dos-io-tests.json":
            require(report.get("checks", 0) >= 655368 and report.get("configurations") == 8 and
                    report.get("aliasBits") == 6 and report.get("realControllerDecoders") is True and
                    report.get("referenceDispatchEquivalent") is True,
                    "DOS video-port dispatch equivalence checks missing")
        else:
            require(report.get("component") == "dos-runtime-rv32-instructions" and
                    report.get("baselineCommit") == PREVIOUS_COMMIT, "DOS runtime baseline differs")
            rows = report.get("rows", [])
            require(len(rows) == 19 and {row.get("case") for row in rows} ==
                    {"divide", "remainder", "aligned-copy", "same-unaligned-copy",
                     "different-alignment-copy", "aligned-fill", "backward-overlap",
                     "pit-square-wave-sample"},
                    "DOS target runtime comparison coverage differs")
            require(all(row.get("beforeInstructions", 0) > 0 and row.get("afterInstructions", 0) > 0
                        for row in rows), "DOS target instruction counts are missing")
        result[name] = {"passed": True, "sha256": sha(raw), "sourceFilesVerified": len(report["sourceHashes"])}
    return result


def verify_dos_startup(read):
    prefix = SKETCH + "tools/native-apps/dos/"
    raw = read("qa/dos-startup-tests.json")
    report = json.loads(raw)
    require(report.get("passed") is True, "DOS serialized startup regression failed")
    scenarios = report.get("scenarios", [])
    require(len(scenarios) == 4 and all(
                type(case.get("french")) is bool and type(case.get("mouse_driver")) is bool
                and case.get("passed") is True for case in scenarios),
            "DOS startup language/driver results are missing or invalid")
    require({(case["french"], case["mouse_driver"]) for case in scenarios} ==
            {(False, False), (False, True), (True, False), (True, True)},
            "DOS startup must cover all four French/US and driver-present/missing combinations")
    for case in scenarios:
        marker = (r"STARTUP PASS french=" + str(int(case["french"])) +
                  r" driver=" + str(int(case["mouse_driver"])) +
                  r" frames=([0-9]+): full BLASTER value, serial driver, C: launch and adjacent file")
        match = re.fullmatch(marker, case.get("stdout", "").strip())
        require(match is not None and 0 < int(match.group(1)) <= 6000,
                "DOS startup guest completion marker is missing or disagrees with its scenario")
    required = {prefix + name for name in (
        "app.c", "startup.h", "core/ibmpc/keyboard.c", "tests/startup_host.c",
        "tests/build_host.py", "tests/run_startup_tests.py")}
    hashes = report.get("sourceHashes", {})
    require(required <= set(hashes), "DOS serialized startup source manifest is incomplete")
    for name, digest in hashes.items():
        safe(name)
        require(sha(read(name)) == digest, "DOS startup validation source differs: " + name)
    return {"passed": True, "scenarios": len(scenarios), "reportSha256": sha(raw),
            "sourceFilesVerified": len(hashes)}


def verify_dos_peripherals(read):
    prefix = SKETCH + "tools/native-apps/dos/"
    mouse_raw = read("qa/mouse-tests.json")
    mouse = json.loads(mouse_raw)
    require(mouse.get("passed") is True, "DOS mouse host regressions failed")
    tests = mouse.get("tests", [])
    require(len(tests) == 2 and all(case.get("passed") is True for case in tests) and
            {case.get("component") for case in tests} == {
                "DOS touchpad, physical mouse and ordered event queue",
                "Real FreeDOS/CuteMouse serial IRQ4 and INT33"},
            "DOS mouse frontend/guest driver coverage differs")
    driver_test = next(case for case in tests if "INT33" in case["component"])
    require(driver_test.get("checks", 0) >= 16 and driver_test.get("failures") == 0,
            "DOS real mouse driver checks failed")
    require(mouse.get("imageSha256") == sha(read(SD + "DOS/FREEDOS.IMG")) and
            mouse.get("driversSha256") == sha(read(SD + "System/DOS/DRIVERS.IMG")),
            "DOS mouse regression used different boot/driver images")
    require({prefix + name for name in ("app_mouse.h", "tests/mouse_frontend.cpp",
            "tests/mouse_host.c", "tests/run_mouse_tests.py")} <= set(mouse.get("sourceHashes", {})),
            "DOS mouse source manifest is incomplete")
    for source, expected in mouse["sourceHashes"].items():
        safe(source)
        require(sha(read(source)) == expected, "DOS mouse validation source differs: " + source)
    audio_raw = read("qa/dos-audio-tests.json")
    audio = json.loads(audio_raw)
    require(audio.get("passed") is True and audio.get("sample_rate") == 22050 and
            audio.get("core_checks", 0) >= 168226 and audio.get("opl_pcm_hash") == "91f11fc9"
            and audio.get("audio_output_passed") is True
            and "OPL NATIVE EQUIVALENCE PASS" in audio.get("core_stdout", ""),
            "DOS sound core/output/equivalence regression failed")
    timelines = audio.get("timeline_scenarios", [])
    expected_timelines = {(speed, jitter, 0) for speed in (56, 79, 100) for jitter in (0, 1)}
    expected_timelines |= {(56, 2, 0), (56, 1, 4294966795)}
    require(len(timelines) == 8 and
            {(case.get("speed"), case.get("jitter"), case.get("origin")) for case in timelines}
            == expected_timelines and audio.get("output_checks", 0) >= 40000 and
            audio.get("output_sanitizers") == "address,undefined",
            "DOS audio slow-guest, jitter and clock-wrap coverage missing")
    require(all(case.get("missing") == 0 and case.get("rebuffers") == 0
                for case in timelines if case["jitter"] != 2),
            "DOS audio queue starved in steady or moderate-jitter tests")
    require(all(0 <= case.get("max_latency_ms", -1) < 400 and
                0 <= case.get("startup_ms", -1) < 500 for case in timelines),
            "DOS audio queue latency is unbounded")
    stall = audio.get("long_stall", {})
    require(stall.get("rebuffers") == 1 and 0 <= stall.get("recovery_ms", -1) < 500,
            "DOS audio did not recover from a long producer stall")
    progression = audio.get("pcm_progression", {})
    require(progression.get("played", 0) > 22050 and progression.get("changed", 0) > 22050 and
            progression.get("positive", 0) > 0 and progression.get("negative", 0) > 0,
            "DOS audio tests did not preserve a changing PCM signal")
    require({"app_audio.h", "core/ibmpc/sound.c", "core/ibmpc/speaker.c", "core/ibmpc/pit.c",
             "core/emu8950/emu8950.c", "tests/audio_host.c", "tests/audio_output_host.c",
             "tests/run_audio_tests.py"} <= set(audio.get("source_sha256", {})),
            "DOS audio source manifest is incomplete")
    for source, expected in audio["source_sha256"].items():
        safe(source)
        require(sha(read(prefix + source)) == expected, "DOS sound validation source differs: " + source)
    return {"passed": True, "mouseReportSha256": sha(mouse_raw), "audioReportSha256": sha(audio_raw),
            "mouseSourceFilesVerified": len(mouse["sourceHashes"]),
            "audioSourceFilesVerified": len(audio["source_sha256"])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "artifact", "package", "output", "config", "previous-package"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    config = json.loads(args.config.read_bytes())
    required_config = {"commit", "tree", "artifactSha256", "artifactId", "workflowRun",
                       "factoryFiles", "embeddedResources", "nativeApplications"}
    require(required_config <= set(config), "Candidate config is incomplete")
    for field, size in (("commit", 40), ("tree", 40), ("artifactSha256", 64)):
        require(isinstance(config[field], str) and
                re.fullmatch("[0-9a-f]{" + str(size) + "}", config[field]) is not None,
                "Invalid candidate config field: " + field)
    for field in ("artifactId", "factoryFiles", "embeddedResources", "nativeApplications"):
        require(type(config[field]) is int and config[field] > 0, "Invalid config count: " + field)
    require(config["nativeApplications"] == len(APPS) == 12, "Expected the 12-application candidate")
    require(config["factoryFiles"] >= 187, "DOS driver resources missing from configured Factory")
    require(isinstance(config["workflowRun"], str) and
            re.fullmatch(r"https://github.com/Belekos/Tab5OS/actions/runs/[0-9]+", config["workflowRun"]),
            "Invalid workflow run URL")
    COMMIT, BUILD_SHA = config["commit"], config["artifactSha256"]
    BUILD_RUN = config["workflowRun"].rsplit("/", 1)[1]
    PREFIX = "Tab5OS_" + VERSION + "_" + COMMIT[:7] + "_complete/"
    require(subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=args.repo).decode().strip()
            == config["tree"], "Source tree differs from candidate config")
    require(file_sha(args.previous_package) == PREVIOUS_SHA, "Wrong exact previous complete package")
    with zipfile.ZipFile(args.previous_package) as previous:
        previous_names = inventory(previous)
        require(all(n.startswith(PREVIOUS_PREFIX) for n in previous_names), "Wrong previous package root")
        previous_sd = PREVIOUS_PREFIX + SD
        require(json.loads(previous.read(PREVIOUS_PREFIX + "BUILD_INFO.json"))["validatedCommit"] == PREVIOUS_COMMIT,
                "Wrong previous source commit")
        previous_factory = json.loads(previous.read(previous_sd + FACTORY + "manifest.json"))
        previous_hashes = previous_factory["files"]
        previous_rules = assignments(previous.read(previous_sd + "MicroPython/lib/tab5factory.py"))
        require(previous_factory["version"] == VERSION and len(previous_hashes) == PREVIOUS_FACTORY_FILES,
                "Unexpected previous Factory manifest")
        require(previous_rules["VERSION"] == VERSION and set(previous_rules["FILES"]) == set(previous_hashes),
                "Previous Factory helper differs from its manifest")
        for name, digest in previous_hashes.items():
            safe(name)
            require(sha(previous.read(previous_sd + name)) == digest and
                    sha(previous.read(previous_sd + FACTORY + name)) == digest,
                    "Previous delivered live/Factory bytes differ: " + name)
        previous_dos = {name[len(previous_sd):]: previous.read(name) for name in previous_names
                        if name.startswith(previous_sd + "DOS/")}
        require(len(previous_dos) == 5 and "DOS/FREEDOS.IMG" in previous_dos,
                "Previous DOS session resources are incomplete")
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
        require(json.loads(read("qa/package-config.json")) == config, "Packaged candidate config differs")
        host_verification = verify_host_reports(read)
        dos_boot_verification = verify_dos_boot(read)
        dos_native_verification = verify_dos_native(read)
        dos_execution_verification = verify_dos_execution(read)
        dos_optimization_verification = verify_dos_optimizations(read)
        dos_peripheral_verification = verify_dos_peripherals(read)
        dos_startup_verification = verify_dos_startup(read)
        info = json.loads(read("BUILD_INFO.json"))
        require(info["validatedCommit"] == COMMIT and info["validatedTree"] == config["tree"],
                "BUILD_INFO source identity differs")
        require(info["artifactId"] == config["artifactId"], "BUILD_INFO artifact ID differs")
        require(info["workflowRun"].rstrip("/").endswith("/" + BUILD_RUN), "Build workflow differs")
        require(info["artifactSha256"] == BUILD_SHA, "BUILD_INFO artifact digest differs")
        require(info["dosSha256"] == dos_native_verification["elfSha256"] and
                info["dosBytes"] == dos_native_verification["elfBytes"], "DOS BUILD_INFO checksum/size differs")
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
        require(len(payloads[".bin"]) == firmware["firmwareBytes"] == info["firmwareBytes"] and
                0 < len(payloads[".bin"]) <= firmware["otaPartitionBytes"],
                "Firmware size differs")
        require(sha(payloads[".bin"]) == info["firmwareSha256"], "Firmware BUILD_INFO checksum differs")
        require(info["otaPartitionBytes"] == firmware["otaPartitionBytes"] == 6553600, "OTA partition size differs")
        require(info["otaFreeBytes"] == firmware["otaFreeBytes"] ==
                firmware["otaPartitionBytes"] - len(payloads[".bin"]) and info["otaFreeBytes"] >= 0,
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
            require({name for name in live if name.startswith("DOS/")} == set(previous_dos),
                    "Default DOS mount layout changed and would hide previous prompt snapshots")
            for name, raw in previous_dos.items():
                require(sd.read("sdcard/" + name) == raw,
                        "DOS boot/session base changed from the delivered candidate: " + name)
            tracked_sd = {name[len(SD):] for name in tracked if name.startswith(SD)}
            native_overlays = {"Applications/Doom.app.elf", "Applications/DOS.app.elf"}
            require(live - tracked_sd - native_overlays == LARGE, "Unexpected baseline-only SD assets")
            require(set(info["unchangedLargeAssetsFromBaseline"]) == LARGE, "Baseline asset provenance differs")
            for name in LARGE:
                require(sd.read("sdcard/" + name) == baseline.read("sdcard/" + name), "Large baseline asset differs: " + name)
            for name in tracked_sd - native_overlays:
                require(sd.read("sdcard/" + name) == (args.repo / SD / name).read_bytes(), "Tracked SD content differs: " + name)
            factory = json.loads(sd.read("sdcard/" + FACTORY + "manifest.json"))
            rules = assignments(sd.read("sdcard/MicroPython/lib/tab5factory.py"))
            require(factory["version"] == rules["VERSION"] == VERSION, "Factory version differs")
            require(len(factory["files"]) == len(rules["FILES"]) == info["factoryFiles"] == config["factoryFiles"],
                    "Factory managed file count differs")
            require(set(factory["files"]) == set(rules["FILES"]), "Factory managed sets differ")
            for name, history in previous_rules["PREVIOUS"].items():
                require(set(history) <= set(rules["PREVIOUS"].get(name, ())),
                        "Previously supported official update history removed: " + name)
            require({n for n in nested if n.startswith(FACTORY)} ==
                    {FACTORY + n for n in factory["files"]} | {FACTORY + "manifest.json"}, "Factory inventory differs")
            old_factory = json.loads(baseline.read("sdcard/" + FACTORY + "manifest.json"))
            require(set(previous_hashes) <= set(factory["files"]), "Previously delivered Factory resources removed")
            dos_resources = {"Applications/DOS.app.elf", "DOS/FREEDOS.IMG", "DOS/README.txt",
                             "DOS/Examples/HELLO.COM", "DOS/Examples/HELLO.EXE", "DOS/Examples/DATA.TXT",
                             "System/DOS/DRIVERS.IMG"}
            require(dos_resources <= set(factory["files"]), "DOS Factory resources missing")
            require(not any(n.startswith("Saves/") for n in factory["files"]), "Personal saves included in Factory")
            for name, digest in factory["files"].items():
                require(sha(sd.read("sdcard/" + name)) == digest, "Live resource differs from Factory hash: " + name)
                require(sha(sd.read("sdcard/" + FACTORY + name)) == digest, "Factory resource hash differs: " + name)
                for old in {old_factory["files"].get(name), previous_hashes.get(name)} - {None, digest}:
                    require(old in rules["PREVIOUS"].get(name, ()),
                            "Delivered official resource missing from repair history: " + name)
            apps = {name for name in live if name.startswith("Applications/") and name.endswith(".app.elf")}
            require(apps == {"Applications/" + app + ".app.elf" for app in APPS}, "Application set differs")
            require(len(apps) == info["nativeApplications"] == config["nativeApplications"], "Application count differs")
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
            require(len(seeds) == info["embeddedResourcesMatched"] == config["embeddedResources"], "Embedded resource count differs")
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
            "factoryFiles": len(factory["files"]), "nativeApplications": app_hashes, "baselineAssets": sorted(LARGE),
            "embeddedResourcesMatched": len(seeds), "firmwareSha256": info["firmwareSha256"],
            "firmwareBytes": info["firmwareBytes"], "otaFreeBytes": info["otaFreeBytes"],
            "testWadBundled": False, "inspectorSha256": file_sha(Path(__file__)),
            "configSha256": file_sha(args.config), "previousPackageSha256": PREVIOUS_SHA,
            "previousFactoryFilesVerified": len(previous_hashes),
            "dosSessionBasePreserved": {name: sha(raw) for name, raw in sorted(previous_dos.items())},
            "hostVerification": host_verification,
            "dosBootVerification": dos_boot_verification,
            "dosNativeVerification": dos_native_verification,
            "dosExecutionVerification": dos_execution_verification,
            "dosOptimizationVerification": dos_optimization_verification,
            "dosPeripheralVerification": dos_peripheral_verification,
            "dosStartupVerification": dos_startup_verification,
            "limitations": ["Archive and build consistency verified; no physical Tab5 test."],
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

