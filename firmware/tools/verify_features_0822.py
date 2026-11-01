"""Verify current 0.8.22 evidence without relabelling historical test results.

Run after the final source freeze and the individual report generators. A stale
or missing report produces a failed features-0822.json and a nonzero exit code.
Compilation, firmware validation and packaging have their own additional gates.
"""
from __future__ import annotations

import datetime
import hashlib
import json
from pathlib import Path
import re

VERSION = "0.8.22"
CURRENT_REPORTS = (
    "wifi-transitions-0822.json", "network-gate-0822.json",
    "directory-picker-0822.json", "ui-controls-0822.json", "calculator-0822.json",
    "reader-layout-0822.json", "terminal-0822.json", "launch-radio-0822.json",
    "terminal-stack-0822.json", "shell-review-0822.json",
    "glyphs-0822.json", "arabic-0822.json", "languages-0822.json",
    "delta-languages-0822.json", "ui-locale-calls-0822.json",
    "syntax-0822.json", "resources-0822.json",
    "maps-0822.json", "gamepad-0822.json", "wiki-flow-0822.json", "reader-seek-0822.json",
    "pentest-lifecycle-0822.json", "pentest-ui-0822.json", "task-diagnostics-0822.json",
    "share-admission-0822.json", "printer-network-gate-0822.json", "memory-target-stack-0822.json",
    "resource-inventory-0822.json", "sdk-0822.json",
    "quota-reservation-0822.json", "git-0822.json", "git-journal-0822.json",
    "git-pull-0822.json", "git-view-0822.json",
    "git-tls-lifecycle-0822.json", "git-init-oom-0822.json",
    "git-regexp-quota-0822.json", "git-stack-0822.json",
    "ssh-0822.json", "ssh-target-0822.json", "libgit2-vendor-0822.json",
    "git-https-0822.json",
    "terminal-worker-0822.json",
    "git-vendor-compile-0822.json",
    "git-no-thread-tls-0822.json", "elf-lifecycle-0822.json",
    "git-config-oom-0822.json",
)
# Byte-identical copies of the released 0.8.19 reports. Their source hashes must
# also still match; dates, labels and historical language-pack hashes stay intact.
HISTORICAL_REPORTS = {
    "firmware-install-0819.json": "cee1a3d082c40530fc22c8cb638b61ec4e81bcae9b4def3b7163ddb40de369b0",
    "pentest-buffer-0819.json": "8bbe6e0265bd741c3f6da83fb6b50467c08191ef29852eabfcee351bc46a499c",
    "keyboard-shortcuts-0819.json": "f1ca360461f0107c370f69c88bc90c4dc49e01b429d3f8b0e181ac03be1c3e02",
    "ime-0819.json": "49d6113892d7e8524a699a159a0468677c62fa89fc6d770991eb883003d6bf56",
    "ime-native-0819.json": "3410aa24f27c232e6f91f9d3b96f696efa6c4529cb4bcb346650cc42d37a840b",
}
LOCALES = {"en-US", "fr-FR", "de-DE", "es-ES", "it-IT", "nl-NL", "pt-BR",
           "pt-PT", "id-ID", "ru-RU", "ar", "zh-CN", "ja-JP", "ko-KR"}
SYNTAX_UNITS = {f"Tab5OS_Arduino_v{VERSION}_Ecriture.ino", "Tab5Audio.cpp",
                "Tab5FileShare.cpp", "Tab5NetworkGate.cpp", "Tab5Tools.cpp",
                "Tab5DirectoryView.cpp", "Tab5FilePicker.cpp", "Tab5Writing.cpp",
                "Tab5UiStyle.cpp", "Tab5ThemePreview.cpp", "Tab5Calculator.cpp",
                "Tab5Shell.cpp", "Tab5Terminal.cpp", "Tab5NetworkTools.cpp", "Tab5NetworkHttp.cpp", "Tab5Pentest.cpp",
                "Tab5PythonReader.cpp", "Tab5Map.cpp", "Tab5Git.cpp", "Tab5GitPort.cpp", "Tab5GitPull.cpp", "Tab5GitView.cpp",
                "Tab5Ssh.cpp", "Tab5Memory.cpp", "Tab5Diagnostics.cpp", "Tab5ResourceManager.cpp", "Tab5PrinterDiscovery.cpp", "WriterUsb.cpp", "Tab5Executable.cpp"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def safe_path(base, name):
    require(isinstance(name, str) and name, "Empty manifest path")
    path = (base / name.replace("\\", "/")).resolve()
    require(path.is_relative_to(base.resolve()), "Unsafe manifest path: " + name)
    return path


def hashes(base, mapping):
    require(isinstance(mapping, dict) and mapping, "Missing source/resource hashes")
    for name, expected in mapping.items():
        require(digest(safe_path(base, name)) == expected, "Hash mismatch: " + name)


def manifest_keys(mapping, required, exact=False):
    require(isinstance(mapping, dict), "Missing manifest")
    require(all(isinstance(name, str) for name in mapping), "Non-string manifest path")
    names = [name.replace("\\", "/") for name in mapping]
    require(len(set(names)) == len(names), "Duplicate normalized manifest paths")
    require(set(names) == set(required) if exact else set(required) <= set(names),
            "Incomplete manifest: " + ", ".join(sorted(set(required) - set(names))))


def git_vendor_rows(reports, sketch):
    manifest = read_json(reports / "libgit2-vendor-0822.json")
    rows = manifest.get("files", [])
    require(rows and len({row["path"] for row in rows}) == len(rows), "Invalid Git vendor inventory")
    for row in rows:
        hashes(sketch / "src/tab5git", {row["path"]: row["embedded_sha256"]})
        require(re.fullmatch(r"[0-9a-f]{64}", row.get("patched_sha256", "")),
                "Missing tested-source digest: " + row["path"])
        require(row.get("generated") is True or re.fullmatch(r"[0-9a-f]{64}", row.get("upstream_sha256", "")),
                "Missing pristine upstream identity: " + row["path"])
    return {row["path"]: row for row in rows}


def tested_git_vendor(reports, sketch, tested):
    """Bind pre-vendoring test input to the final, include-rewritten source."""
    rows = git_vendor_rows(reports, sketch)
    for name, expected in tested.items():
        matches = [row for row in rows.values() if row["upstream"] == name]
        require(matches and all(row["patched_sha256"] == expected for row in matches),
                "Tested Git source differs from final vendor input: " + name)


def git_worker_stack(sketch):
    text = (sketch / "Tab5Git.h").read_text(encoding="utf-8")
    match = re.search(r"WorkerStackBytes\s*=\s*(\d+)u?\s*\*\s*1024u?", text)
    require(match, "Unrecognized current Git worker stack constant")
    return int(match[1]) * 1024


SSH_EXPECTED = {
    "keyscan": 0, "unknown host fails before authentication": 1, "explicit trust": 0,
    "TCP waits through 10 empty readiness polls": 0, "stdout and stderr remain distinct": 0,
    "pipeline stdin 200KiB": 0, "simultaneous streams above receive window": 0,
    "remote exit status": 7, "wrong password": 1, "PEM ECDSA private key": 0,
    "encrypted PEM ECDSA private key": 0, "PEM RSA private key with SHA2 signature": 0,
    "malformed PEM is rejected": 1, "wrong PEM passphrase is rejected": 1,
    "missing ECDSA public companion is rejected": 1, "changed fingerprint cannot overwrite trust": 1,
    "forget": 0, "pin wrong fingerprint": 0, "changed server key blocks credentials": 1,
    "forget wrong pin": 0, "restore correct pin": 0, "cancellation of silent server": 130,
    "idle timeout": 1, "network disabled": 130, "network gate busy": 1, "SFTP listing": 0,
    "SFTP receive 1MiB": 0, "SFTP refuses overwrite local": 1, "SFTP send 1MiB": 0,
    "SFTP refuses overwrite remote": 1, "SFTP CLOSE failure consumes handle once": 1,
    "SFTP cancelled receive removes owned partial": 130,
    "50 sessions in one process retain no CRT allocation": 0,
    "20 ECDSA sessions retain no CRT allocation": 0,
}
PULL_CASES = {
    "normal fast-forward", "process exit at intent:",
    *("process exit at checkout-file:" + str(n) for n in range(1, 6)),
    *("process exit at " + point + ":" for point in ("checkout", "index", "head", "cleanup")),
    *("preserve " + part for part in ("a.txt", "added.txt", "delete.txt", "staged", "branch", "missing", "corrupt")),
    *("cancellation callback " + str(n) for n in (1, 2, 10)),
    *("cancel at " + point for point in ("checkout", "index", "head")),
    "preserve unrelated and untracked files",
}
GIT_CRITICAL_VENDOR = {"src/libgit2/config_file.c", "src/libgit2/config_list.c", "src/util/thread.c",
                       "src/libgit2/streams/mbedtls.c"}


def source_manifest(sketch):
    return {p.name: digest(p) for p in sorted(sketch.iterdir())
            if p.is_file() and p.suffix in {".ino", ".cpp", ".h", ".inc"}}


def dependency_manifest(sketch):
    return {p.relative_to(sketch).as_posix(): digest(p)
            for name in ("tab5git", "tab5ssh") for p in sorted((sketch / "src" / name).rglob("*"))
            if p.is_file()}


def check_count(report, minimum):
    match = re.search(r"PASS (\d+) (?:real decoder )?checks", report.get("output", ""))
    require(match and int(match[1]) >= minimum,
            "Missing successful checks: expected at least " + str(minimum))


def checked_report(name, report, release, sketch, reports):
    sd = sketch / "sdcard"
    if name in HISTORICAL_REPORTS:
        require(digest(reports / name) == HISTORICAL_REPORTS[name],
                "Historical report was modified: " + name)
    elif name != "libgit2-vendor-0822.json" and ("version" in report or "release" in report):
        require(report.get("version", report.get("release")) == VERSION,
                "Wrong release in " + name)
    success = report.get("pass") if name == "arabic-0822.json" else report.get("passed")
    if name == "resources-0822.json":
        success = report.get("result") == "PASS"
    elif name == "libgit2-vendor-0822.json":
        # A provenance manifest is not a test execution report. Validate it
        # below instead of manufacturing a successful test flag in that file.
        success = True
    require(success is True, "Report did not pass: " + name)

    if name == "wifi-transitions-0822.json":
        current, baseline = report["current"], report["baseline_0_8_19"]
        require(current.get("passed") is True and current.get("exit_code") == 0,
                "Current Wi-Fi transitions did not pass")
        require(baseline.get("expected_failure_observed") is True and
                baseline.get("exit_code") == 1 and
                baseline.get("output", "").count("EXPECTED-FAIL:") == 3,
                "Missing the three reproduced 0.8.19 Wi-Fi failures")
        require(current.get("sources") == report.get("sources"), "Wi-Fi manifests disagree")
        hashes(sketch, report["sources"])
    elif name == "network-gate-0822.json":
        require(report.get("status") == "PASS", "Network gate did not pass")
        hashes(sketch, report["sources"])
        require(digest(sketch / "Tab5NetworkGate.cpp") == report.get("source_sha256"),
                "Network gate source changed")
    elif name == "directory-picker-0822.json":
        require(report.get("exitCode") == 0, "Directory/picker test failed")
        check_count(report, 540)
        hashes(sketch, report["sources"])
    elif name in {"ui-controls-0822.json", "calculator-0822.json"}:
        if name == "ui-controls-0822.json":
            require(report.get("exitCode") == 0, "Native UI test failed")
        check_count(report, 250 if name.startswith("ui-") else 17858)
        hashes(release, report["sources"])
    elif name == "reader-layout-0822.json":
        baseline, current = report["baseline"], report["current"]
        require(baseline.get("passed") is False and baseline.get("exitCode") == 1 and
                baseline.get("expected_failure_observed") is True and
                "FAIL actual rendered last line fits the page" in baseline.get("output", ""),
                "Missing reproduced .20 bottom-line clipping")
        require(current.get("passed") is True and current.get("exitCode") == 0,
                "Current real-LVGL pagination failed")
        check_count(current, 7516)
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware claim")
        hashes(release, report["sources"])
    elif name == "terminal-0822.json":
        require(report.get("assertions", 0) >= 68 and
                report.get("repeated_sessions", 0) >= 2000 and
                report.get("repeated_commands", 0) >= 2000 and
                report.get("retained_heap_bytes") == 0,
                "Terminal shell/memory stress evidence is incomplete")
        require(report.get("physical_device_tested") is False, "Unexpected hardware claim")
        hashes(sketch, report["sources"])
        require(digest(release / "tools/terminal-tests/terminal.cpp") == report.get("harness_sha256"),
                "Terminal harness changed after stress test")
        regression = report["existing_commands_regression"]
        require(regression.get("passed") is True and regression.get("exit_code") == 0 and
                "PASS:" in regression.get("output", ""), "Existing terminal commands regressed")
        require(regression.get("runner") == "tools/network-tests/run_shell.py",
                "Unexpected existing-command regression runner")
        hashes(release, {"tools/network-tests/run_shell.py": regression["runner_sha256"],
                         "tools/network-tests/shell.cpp": regression["harness_sha256"],
                         "tools/network-tests/ShellPlatform.h": regression["platform_sha256"]})
        for key, runner in (("network_regression", "run.py"), ("http_regression", "run_http.py")):
            regression = report[key]
            require(regression.get("passed") is True and regression.get("exit_code") == 0 and
                    "PASS" in regression.get("output", "") and
                    regression.get("runner") == "tools/network-tests/" + runner,
                    "Network/HTTP regression or diagnostics checks failed: " + key)
            hashes(release / "tools/network-tests", regression["sources"])
    elif name == "launch-radio-0822.json":
        for part, minimum in (("native", 31), ("radio", 24)):
            row = report[part]
            require(row.get("exitCode") == 0, "Launch/radio test failed: " + part)
            match = re.search(r"PASS (\d+) ", row.get("output", ""))
            require(match and int(match[1]) >= minimum, "Incomplete launch/radio checks: " + part)
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware claim")
        hashes(sketch, report["sources"])
        hashes(release, report["tests"])
    elif name == "terminal-stack-0822.json":
        hashes(sketch, report["sources"])
        hashes(sketch, report["compilationInputs"])
        require(report.get("source") == "Tab5Shell.cpp" and
                report.get("sourceSha256") == digest(sketch / "Tab5Shell.cpp"),
                "Terminal target stack source changed")
        require(report.get("workerStackBytes") == 16384 and report.get("maximumBlocks") == 6 and
                report.get("maximumSubstitutions") == 4 and
                report.get("maximumNonSubstitutionBlocks") == 4 and
                report.get("remainingBeforePlatformFrames", 0) >= 2048 and
                report.get("physicalDeviceTested") is False,
                "Target stack review or scope is incomplete")
        frames = report.get("frames", [])
        require(frames and all(row.get("kind") == "static" for row in frames),
                "Missing target static stack frames")
        require(report["knownShellAndDirectoryBytes"] + report["remainingBeforePlatformFrames"] == 16384,
                "Inconsistent target stack accounting")
    elif name == "shell-review-0822.json":
        require(report.get("checks", 0) >= 21 and report.get("failures") == 0 and
                report.get("hardwareTested") is False, "Independent shell review failed")
        hashes(release, report["sources"])
    elif name == "glyphs-0822.json":
        hashes(sketch, report["sources"])
        hashes(sd, report["assets"])
        require(set(report["packs"]) == LOCALES and
                all(p.get("missingGlyphs") == [] for p in report["packs"].values()),
                "Current pack glyph coverage is incomplete")
        require(all(report.get("calculatorSymbols", {}).get(c) is True for c in "π√×÷−"),
                "Calculator glyph evidence is incomplete")
        require(report.get("physicalDisplayTested") is False, "Unexpected hardware-test claim")
    elif name == "arabic-0822.json":
        require(report.get("unsupportedShapedGlyphs") == {} and
                report.get("physicalDisplayTested") is False, "Arabic shaping evidence failed")
        require(digest(sd / "M5Writer/languages/ar.json") == report.get("packSha256") and
                digest(sd / "M5Writer/font.ttf") == report.get("fontSha256"),
                "Arabic pack or font changed after shaping tests")
        require(report.get("sampleCount", 0) >= report.get("arabicPackEntries", 0) > 0,
                "Incomplete Arabic shaping samples")
    elif name == "languages-0822.json":
        keys = set(read_json(sd / "M5Writer/languages/en-US.json")["translations"])
        require(len(keys) == report.get("referenceEntries") and set(report["packs"]) == LOCALES,
                "Current reference/language inventory differs")
        for locale, audit in report["packs"].items():
            require(audit.get("missingKeys") == 0 and audit.get("coveragePercent") == 100.0,
                    "Incomplete language: " + locale)
            for field in ("formatErrors", "lineBreakErrors", "untranslatedLongText", "unsupportedGlyphs"):
                require(audit.get(field) == [], "Language audit failed: " + locale + "/" + field)
            path = sd / "M5Writer/languages" / (locale + ".json")
            require(digest(path) == audit.get("sha256") and path.stat().st_size == audit.get("bytes"),
                    "Pack changed after audit: " + locale)
            pack = read_json(path)
            require(keys <= pack["translations"].keys() and pack.get("keyboard") == locale and
                    audit.get("keyboardAvailable") is True, "Pack/keyboard mismatch: " + locale)
    elif name == "delta-languages-0822.json":
        hashes(release / "tools/localization", report["deltaFiles"])
        require(set(report["packs"]) == LOCALES, "Incomplete new-key translation inventory")
        for locale, audit in report["packs"].items():
            require(audit.get("translatedNewKeys") == report.get("newSourceKeys") and
                    digest(sd / "M5Writer/languages" / (locale + ".json")) == audit.get("sha256"),
                    "Current translation delta differs: " + locale)
    elif name == "ui-locale-calls-0822.json":
        require(report.get("missingKeys") == {}, "Untranslated literal callsites")
        hashes(sketch, report["sourceHashes"])
        # The literal-call audit excludes the two generated English tables.
        # They are still bound by this gate's complete top-level source manifest.
        audited_units = set(source_manifest(sketch)) - {"Tab5EnglishBitmap.h", "Tab5EnglishFallback.h"}
        require(set(report["sourceHashes"]) == audited_units,
                "Locale audit does not cover the current source unit inventory")
        languages = read_json(reports / "languages-0822.json")
        require(report.get("referenceEntries") == languages.get("referenceEntries"),
                "Locale audit reference changed")
    elif name == "syntax-0822.json":
        rows = report.get("files", [])
        names = [row["name"] for row in rows]
        require(SYNTAX_UNITS <= set(names) and len(names) == len(set(names)),
                "Missing or duplicate current target-syntax units")
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware-test claim")
        for row in rows:
            require(row.get("exitCode") == 0 and row.get("sourceUnchanged") is True,
                    "Target syntax failed or source changed: " + row["name"])
            hashes(sketch, {row["name"]: row["sha256"]})
    elif name == "resources-0822.json":
        require(all(report.get(k) is True for k in ("payload_matches_sd", "python_syntax", "retired_apps_absent")),
                "Incomplete resource checks")
        factory_root = sd / f"Factory/Tab5OS-{VERSION}"
        factory = read_json(factory_root / "manifest.json")
        require(factory.get("version") == VERSION and
                len(factory["files"]) == report.get("factory_resources"), "Factory manifest differs")
        hashes(sd, factory["files"])
        hashes(factory_root, factory["files"])
        header = (sketch / "Tab5SystemResourcesData.h").read_text(encoding="utf-8")
        blobs = {n: "".join(json.loads(line) for line in literals.splitlines()).encode("utf-8")
                 for n, literals in re.findall(r"static const char TAB5_RESOURCE_(\d+)\[\] =\n(.*?);\n", header, re.S)}
        entries = re.findall(r'\{"([^"]+)", TAB5_RESOURCE_(\d+), sizeof\(TAB5_RESOURCE_\d+\)-1\}', header)
        require(len(entries) == len(blobs) == report.get("embedded_resources") and len(entries) >= 30,
                "Embedded resource inventory differs")
        for resource, number in entries:
            require(blobs[number] == safe_path(sd, resource.lstrip("/")).read_bytes(),
                    "Embedded resource differs from SD: " + resource)
        for path in (sd / "MicroPython").rglob("*.py"):
            compile(path.read_text(encoding="utf-8"), str(path), "exec")
        retired = {"Applications/Quake.app.elf", "Applications/Duke3D.app.elf"}
        require(retired.isdisjoint(factory["files"]) and all(not (sd / n).exists() for n in retired),
                "Retired application remains in the SD resources")
        require(not any("Quake" in p.name or "Duke3D" in p.name for p in (sd / "Factory").rglob("*.elf")),
                "Retired application remains in Factory")
        require(not any((sketch / n).exists() for n in
                        ("Tab5Mail.cpp", "Tab5Browser.cpp", "Tab5HtmlView.cpp", "src/ReadyMail", "src/litehtml")),
                "Retired source module remains")
    elif name in {"maps-0822.json", "wiki-flow-0822.json", "reader-seek-0822.json"}:
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware claim")
        hashes(release, report["sources"])
        if name == "maps-0822.json":
            hashes(release, report["tests"])
            require(len(report["results"]) == 2 and
                    all(row.get("exitCode") == 0 and "PASS" in row.get("output", "")
                        for row in report["results"]), "Map tests failed")
            main = (sketch / (sketch.name + ".ino")).read_text(encoding="utf-8")
            for signature, expected in report["controllers"].items():
                start = main.index(signature)
                end = main.index("{", start) + 1
                depth = 1
                while depth:
                    if main[end] == "{": depth += 1
                    elif main[end] == "}": depth -= 1
                    end += 1
                require(hashlib.sha256(main[start:end].encode()).hexdigest() == expected,
                        "Map controller changed after tests: " + signature)
        elif name == "wiki-flow-0822.json":
            require(report["current"].get("passed") is True and
                    report["current"].get("exitCode") == 0 and
                    "failures=0" in report["current"].get("output", "") and
                    report["baseline"].get("expectedFailure") is True and
                    report["baseline"].get("exitCode") == 1,
                    "Wikipedia current/baseline tests incomplete")
    elif name == "gamepad-0822.json":
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware claim")
        hashes(sketch, report["files"])
        hashes(release / "tools/usb-gamepad-tests", report["harness"])
        require(report["current"].get("exitCode") == 0 and
                "retained allocations 0; claims 0" in report["current"].get("result", "") and
                report["baseline"].get("exitCode") == 1 and
                "6009 failures" in report["baseline"].get("result", "") and
                "PASS" in report.get("enumeration", "") and "PASS" in report.get("packets", ""),
                "USB lifecycle/decoder regression evidence incomplete")
    elif name in {"pentest-lifecycle-0822.json", "pentest-ui-0822.json", "task-diagnostics-0822.json",
                  "share-admission-0822.json", "printer-network-gate-0822.json",
                  "memory-target-stack-0822.json", "resource-inventory-0822.json", "quota-reservation-0822.json",
                  "terminal-worker-0822.json"}:
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware claim")
        hashes(sketch, report["sources"])
        hashes(release / "tools/pentest-tests", report["tests"])
        if name == "pentest-lifecycle-0822.json":
            rows = report["results"]
            require(len(rows) == 2 and all(row.get("exitCode") == 0 for row in rows),
                    "Pentest lifecycle tests failed")
            baseline = next(row for row in rows if row["baseline"])
            current = next(row for row in rows if not row["baseline"])
            require("retained_bytes=1536000" in baseline["output"] and
                    "retained_bytes=0;" in current["output"], "Pentest leak evidence incomplete")
        elif name == "memory-target-stack-0822.json":
            require(len(report["results"]) >= 9 and all(row.get("exitCode") == 0 and row.get("frames")
                    for row in report["results"]), "Target resource compilation/stack evidence incomplete")
        elif name == "resource-inventory-0822.json":
            require(len(report["sources"]) >= 260 and len(report["sites"]) >= 30,
                    "Resource inventory incomplete")
        elif name == "terminal-worker-0822.json":
            manifest_keys(report["sources"], {"Tab5Terminal.cpp", "Tab5Terminal.h", "Tab5Task.h"}, exact=True)
            manifest_keys(report["tests"], {"run_terminal_worker.py", "terminal_worker.cpp"}, exact=True)
            match = re.search(r"PASS (\d+) terminal worker checks; cycles=(\d+); context_owned_after_release=(\d+); tls_writes=(\d+)", report.get("output", ""))
            require(match and int(match[1]) >= 26000 and int(match[2]) >= 2000 and
                    int(match[3]) == 0 and int(match[4]) == 0, "Terminal final-access/SDK TLS regression incomplete")
        else:
            require("PASS" in report.get("output", ""), "Missing successful resource regression output")
    elif name in {"git-0822.json", "git-journal-0822.json", "git-pull-0822.json"}:
        require(report.get("physicalDeviceTested") is False, "Unexpected or unspecified Git hardware scope")
        required = {
            "git-0822.json": {"Tab5Git.cpp", "Tab5Git.h", "Tab5GitJournal.h", "Tab5GitPort.cpp",
                              "Tab5GitPort.h", "Tab5TerminalPath.h", "Tab5FilePolicy.h",
                              "Tab5GitPull.cpp", "Tab5GitPull.h", "git_cli.cpp", "run.py"},
            "git-journal-0822.json": {"Tab5GitJournal.h", "Tab5GitPort.cpp", "journal.cpp", "run.py"},
            "git-pull-0822.json": {"Tab5GitPull.h", "Tab5GitPull.cpp", "pull.cpp", "run_pull.py"},
        }[name]
        manifest_keys(report["sources"], required)
        for source, expected in report["sources"].items():
            require("/" not in source and "\\" not in source, "Unexpected Git basename: " + source)
            base = sketch if source.startswith("Tab5") else release / "tools/git-tests"
            hashes(base, {source: expected})
        require(report.get("checks", 0) >= {"git-0822.json": 45, "git-journal-0822.json": 145,
                "git-pull-0822.json": 25}[name], "Incomplete Git checks")
        if name == "git-pull-0822.json":
            cases = report.get("cases", [])
            require(len(cases) == len(set(cases)) == report["checks"] and PULL_CASES <= set(cases),
                    "Git interruption/preservation/cancellation cases absent or duplicated")
        elif name == "git-0822.json":
            require(report.get("sourceStableDuringTest") is True and report.get("threads") is False,
                    "Git command test sources or no-thread profile changed")
            vendor_required = {sketch.name + "/src/tab5git/" + p for p in GIT_CRITICAL_VENDOR}
            manifest_keys(report["vendorSources"], vendor_required)
            hashes(release, report["vendorSources"])
            crypto = report["cryptoBackend"]
            require(crypto.get("name") == "mbedTLS" and crypto.get("version") == "3.6.5" and
                    crypto.get("sourceCommit") == "ffb280bb63c78bfec1e1ab55040671768c85c923" and
                    re.fullmatch(r"[0-9a-f]{64}", crypto.get("librarySha256", "")),
                    "Git command test crypto provenance differs from the target SDK version")
            commands = report.get("commands", [])
            require(len(commands) == report["checks"], "Git command accounting differs")
            expected = [(["init", "project"], 0), (["add", "-A"], 0),
                        (["add", "--", "a.txt"], 0), (["diff"], 0), (["diff", "--cached"], 0),
                        (["log"], 0), (["init", "/"], 1), (["init", "usb:/"], 1),
                        (["remote", "-v"], 0), (["status"], 130), (["recover", "--continue"], 1)]
            require(all(any(row.get("args") == args and row.get("code") == code for row in commands)
                        for args, code in expected), "Required Git command/negative cases missing")
            require(report.get("repeatedStatusCycles", 0) >= 200,
                    "Git repeated sessions in a single process are missing")
            faults = [row for row in commands if "injectedAllocationFailure" in row]
            require(len(faults) == 60 and {row["injectedAllocationFailure"] for row in faults} == set(range(0, 180, 3)) and
                    any(row.get("code") == 1 for row in faults), "Git command-level OOM/recovery sweep is incomplete")
            for row in faults:
                output = "\n".join(row.get("memory", []))
                memory = re.search(r"TAB5_GIT_MEMORY (\d+) (\d+)", output)
                handles = re.search(r"TAB5_GIT_HANDLES (\d+) (\d+)", output)
                reclaimed = re.search(r"TAB5_GIT_RECLAIMED (\d+) (\d+)", output)
                require(row.get("code") in (0, 1) and memory and int(memory[1]) == 0 and
                        0 <= int(memory[2]) <= 8 * 1024 * 1024 and handles and
                        int(handles[2]) <= int(handles[1]) and reclaimed,
                        "Git OOM retained-memory/handle or explicit arena cleanup evidence failed")
    elif name == "git-tls-lifecycle-0822.json":
        require(report.get("physicalDeviceTested") is False and report.get("assertions", 0) >= 24600 and
                report.get("cycles", 0) >= 1600 and report.get("retainedBytes") == 0 and
                report.get("peakTrackedBytes", 0) > 0, "Git TLS ownership/failure coverage incomplete")
        source_map = {"mbedtls.c": "src/libgit2/streams/mbedtls.c"}
        manifest_keys(report["sources"], {*source_map, "tls_lifecycle.cpp", "run_tls_lifecycle.py"}, exact=True)
        hashes(release / "tools/git-tests", {p: report["sources"][p] for p in
                                            ("tls_lifecycle.cpp", "run_tls_lifecycle.py")})
        tested_git_vendor(reports, sketch, {path: report["sources"][name] for name, path in source_map.items()})
    elif name == "git-init-oom-0822.json":
        require(report.get("physicalDeviceTested") is False and report.get("assertions", 0) >= 6263 and
                report.get("failurePoints", 0) >= 33 and report.get("rounds", 0) >= 3 and
                report.get("initializationFailures", 0) >= 90 and report.get("retainedPayloadBytes") == 0 and
                report.get("peakPayloadBytes", 0) > 0, "Git real-library initialization OOM coverage incomplete")
        source_map = {"sysdir.c": "src/libgit2/sysdir.c", "filter.c": "src/libgit2/filter.c",
                      "merge_driver.c": "src/libgit2/merge_driver.c", "runtime.c": "src/util/runtime.c"}
        manifest_keys(report["sources"], {*source_map, "init_oom.cpp", "run_init_oom.py"}, exact=True)
        hashes(release / "tools/git-tests", {p: report["sources"][p] for p in ("init_oom.cpp", "run_init_oom.py")})
        tested_git_vendor(reports, sketch, {path: report["sources"][name] for name, path in source_map.items()})
        require(re.fullmatch(r"[0-9a-f]{64}", report.get("hostLibrarySha256", "")),
                "Missing identity of the tested host binary (not the P4 binary)")
    elif name == "git-no-thread-tls-0822.json":
        require(report.get("physicalDeviceTested") is False and report.get("checks", 0) >= 119017 and
                report.get("cycles", 0) >= 1000 and report.get("destructorCalls", 0) >= 17002 and
                report.get("simultaneousKeys") == 16, "Git no-thread TLS slot/destructor coverage incomplete")
        manifest_keys(report["sources"], {"thread.c", "no_thread_tls.cpp", "run_no_thread_tls.py"}, exact=True)
        hashes(release / "tools/git-tests", {p: report["sources"][p] for p in ("no_thread_tls.cpp", "run_no_thread_tls.py")})
        tested_git_vendor(reports, sketch, {"src/util/thread.c": report["sources"]["thread.c"]})
    elif name == "git-config-oom-0822.json":
        require(report.get("physicalDeviceTested") is False and report.get("checks", 0) >= 31378 and
                report.get("cycles", 0) >= 432 and report.get("allocationFailures", 0) >= 84 and
                report.get("failurePointsPerOperation", 0) >= 48 and report.get("rounds", 0) >= 3 and
                report.get("retainedPayloadBytes") == 0 and report.get("peakPayloadBytes", 0) > 0,
                "Git real-library config insertion/duplication OOM coverage incomplete")
        source_map = {"config_list.c": "src/libgit2/config_list.c", "thread.c": "src/util/thread.c"}
        manifest_keys(report["sources"], {*source_map, "config_oom.cpp", "run_config_oom.py"}, exact=True)
        hashes(release / "tools/git-tests", {p: report["sources"][p] for p in ("config_oom.cpp", "run_config_oom.py")})
        tested_git_vendor(reports, sketch, {path: report["sources"][name] for name, path in source_map.items()})
        require(re.fullmatch(r"[0-9a-f]{64}", report.get("hostLibrarySha256", "")),
                "Missing identity of the tested host library")
    elif name == "git-regexp-quota-0822.json":
        require(report.get("physicalDeviceTested") is False, "Unexpected Git regexp hardware claim")
        match = re.search(r"PASS (\d+) real-PCRE2 quota/lifecycle/limits checks; cycles=(\d+); retained_bytes=(\d+);", report.get("output", ""))
        require(match and int(match[1]) >= 14173 and int(match[2]) >= 500 and int(match[3]) == 0,
                "Git regexp allocation/quota/cancellation evidence incomplete")
        # Final runner must bind the vendored implementation it actually tests.
        hashes(sketch, report["sources"])
        required = {p.relative_to(sketch).as_posix() for p in (sketch / "src/tab5git/deps/pcre2").glob("*")
                    if p.suffix in {".c", ".h"}}
        required.add("src/tab5git/src/util/regexp.c")
        required.add("src/tab5git/Tab5GitEmbeddedConfig.h")
        manifest_keys(report["sources"], required)
        manifest_keys(report["tests"], {"run_regexp.py", "regexp.cpp", "regexp_config.h"}, exact=True)
        hashes(release / "tools/git-tests", report["tests"])
    elif name == "git-stack-0822.json":
        require(report.get("physicalDeviceTested") is False and report.get("wholeStackBound") is False,
                "Git frame samples must not claim a hardware or whole-stack bound")
        require(report.get("workerStackBytes") == git_worker_stack(sketch) >= 40 * 1024,
                "Git worker stack changed after target review")
        hashes(sketch, report["sources"])
        required = {p.relative_to(sketch).as_posix() for p in (sketch / "src/tab5git").rglob("*")
                    if p.is_file() and p.suffix in {".c", ".h"}}
        required.update({"Tab5Git.cpp", "Tab5Git.h", "Tab5GitPort.cpp", "Tab5GitPort.h", "Tab5GitView.cpp",
                         "Tab5Terminal.cpp", "Tab5Task.h"})
        manifest_keys(report["sources"], required)
        manifest_keys(report["tests"], {"run_stack_audit.py"}, exact=True)
        hashes(release / "tools/git-tests", report["tests"])
        compiled = read_json(reports / "git-vendor-compile-0822.json")
        require(digest(reports / "git-vendor-compile-0822.json") == report.get("compileReportSha256"),
                "Git target compilation changed after frame review")
        require(report.get("compileInputs") == compiled.get("inputs") and report.get("compileInputs") and
                report.get("responseSha256") == compiled.get("responseSha256") and
                report.get("responseFlags") == compiled.get("responseFlags"),
                "Git stack review does not bind the full target compilation inputs/flags")
        hashes(release, report["compileInputs"])
        expected_checks = {"vendorCompileCurrent", "workerStackAtLeast40KiB", "pcreCompileInitialFrameAtMost4KiB",
                           "hashFdFrameAtMost8KiB", "terminalUsesIsolatedWorker"}
        require(expected_checks <= set(report["checks"]) and all(report["checks"][k] is True for k in expected_checks),
                "Git target stack profile checks failed")
        rows = report.get("rows", [])
        by_key = {(row["source"], row["name"]): row for row in rows}
        require(len(rows) == len(by_key) and len(rows) >= 3000 and
                all(row.get("kind") == "static" and row.get("bytes", -1) >= 0 and
                    row.get("source") in report["sources"] for row in rows), "Git static frame inventory invalid")
        entry_names = {"git_libgit2_init", "git_repository_open_ext", "git_clone", "git_remote_fetch",
                       "git_remote_push", "git_index_add_all", "git_commit_create", "git_diff_index_to_workdir",
                       "git_status_foreach_ext", "git_regexp_compile", "git_regexp_match"}
        entries = report.get("entries", [])
        require(len(entries) == len({row["entry"] for row in entries}) and
                entry_names <= {row["entry"] for row in entries}, "Git direct-call path examples missing")
        for entry in entries:
            path = entry.get("path", [])
            require(path and path[0].get("name") == entry["entry"] and
                    all(by_key.get((row["source"], row["name"])) == row for row in path) and
                    sum(row["bytes"] for row in path) == entry.get("frameSum"),
                    "Git direct-call sample does not match measured frames: " + entry["entry"])
        for function, limit in (("pcre2_compile_8", 4096), ("git_odb__hashfd", 8192)):
            frames = [row for row in rows if row["name"] == function]
            require(len(frames) == 1 and frames[0]["bytes"] <= limit, "Git oversized native frame: " + function)
        require(isinstance(report.get("unresolved"), list) and isinstance(report.get("cycles"), list) and
                report["unresolved"] and report["cycles"], "Git unresolved/recursive scope disclosure missing")
        objects = report["objectInputs"]
        expected_objects = {str(Path(p).relative_to("src/tab5git").with_suffix(".o")).replace("\\", "/")
                            for p in required if p.startswith("src/tab5git/") and p.endswith(".c")}
        manifest_keys(objects, expected_objects, exact=True)
        for row in objects.values():
            require(re.fullmatch(r"[0-9a-f]{64}", row.get("objectSha256", "")), "Missing Git target object identity")
            require((row.get("noDefinedTextSymbols") is True and row.get("stackUsageSha256") is None) or
                    (row.get("noDefinedTextSymbols") is False and
                     re.fullmatch(r"[0-9a-f]{64}", row.get("stackUsageSha256", ""))),
                    "Git stack-usage object evidence incomplete")
    elif name == "git-vendor-compile-0822.json":
        require(report.get("physicalDeviceTested") is False and report.get("sourcesStableDuringTest") is True,
                "Git compile hardware scope or source stability unspecified")
        required = {p.relative_to(sketch).as_posix() for p in (sketch / "src/tab5git").rglob("*.c")}
        rows = report.get("results", [])
        require(len(rows) == len(required) == report.get("units") and {row["file"] for row in rows} == required and
                all(row.get("exit") == 0 for row in rows), "Git vendor object compilation incomplete")
        hashes(sketch, {row["file"]: row["sha256"] for row in rows})
        inputs = {p.relative_to(release).as_posix() for p in (sketch / "src/tab5git").rglob("*")
                  if p.is_file() and p.suffix in {".c", ".h", ".inc"}}
        manifest_keys(report["inputs"], inputs, exact=True)
        hashes(release, report["inputs"])
        runner = report["runner"]
        require(runner.get("path") == "tools/git-tests/compile_vendor.py", "Unexpected Git target compiler runner")
        hashes(release, {runner["path"]: runner["sha256"]})
        flags = report.get("responseFlags", "")
        require(re.fullmatch(r"[0-9a-f]{64}", report.get("responseSha256", "")) and
                all(flag in flags for flag in ("-fstack-usage", "riscv32-esp-unknown-elf", "ARDUINO_M5STACK_TAB5")) and
                "ESP32-P4" in report.get("target", ""),
                "Git target toolchain flags/identity missing")
        require(not re.search(r"Tab5OS_(?:Arduino|Ecriture)_v(?!0\.8\.22)[0-9]", flags) and
                not re.search(r"tab5-(?:qa|build)-0821", flags), "Git target flags still reference the previous release")
        require(report["responseSha256"] in {hashlib.sha256(flags.encode()).hexdigest(),
                hashlib.sha256(flags.replace("\n", "\r\n").encode()).hexdigest()},
                "Git recorded target flags differ from response-file identity")
    elif name == "git-https-0822.json":
        require(report.get("physicalDeviceTested") is False, "Unexpected Git HTTPS hardware claim")
        hashes(release, report["sourceHashes"])
        prefix = sketch.name + "/"
        manifest_keys(report["sourceHashes"], {
            prefix + p for p in ("Tab5Git.cpp", "Tab5Git.h", "Tab5GitPort.cpp", "Tab5GitPull.cpp",
                                "src/tab5git/src/libgit2/streams/mbedtls.c", "src/tab5git/UPSTREAM.json")
        } | {"tools/git-tests/https_fixture.py", "tools/git-tests/https_remote.py", "tools/git-tests/git_cli.cpp"})
        build = report["buildEvidence"]
        require(build.get("threads") is False and re.fullmatch(r"[0-9a-f]{64}", build.get("manifestSha256", "")),
                "Git HTTPS tested executable build manifest identity missing")
        crypto = build["cryptoBackend"]
        require(crypto.get("name") == "mbedTLS" and crypto.get("version") == "3.6.5" and
                crypto.get("sourceCommit") == "ffb280bb63c78bfec1e1ab55040671768c85c923" and
                re.fullmatch(r"[0-9a-f]{64}", crypto.get("librarySha256", "")),
                "Git HTTPS native crypto backend differs from the target SDK version")
        linked = build["linkedSources"]
        required_linked = {prefix + p for p in ("Tab5Git.cpp", "Tab5Git.h", "Tab5GitPort.cpp", "Tab5GitPort.h",
                           "Tab5GitPull.cpp", "Tab5GitPull.h", "Tab5GitJournal.h", "Tab5TerminalPath.h", "Tab5FilePolicy.h")}
        required_linked.update({"tools/git-tests/git_cli.cpp", "tools/git-tests/run.py"})
        manifest_keys(linked, required_linked)
        for source, expected_hash in linked.items():
            require(re.fullmatch(r"[0-9a-f]{64}", expected_hash), "Missing Git HTTPS linked input identity: " + source)
            if not source.startswith("build/"):
                require(report["sourceHashes"].get(source) == expected_hash,
                        "Git HTTPS source and linked input manifests disagree: " + source)
        require(crypto["librarySha256"] in {sha for source, sha in linked.items() if source.startswith("build/")},
                "Git HTTPS cryptography library was not present among linked inputs")
        # Native libraries stay outside the portable source package. For the
        # adapted backend preserve both byte identities and bind them to the
        # tested/patched and embedded identities in the final vendor manifest.
        adapted = build["linkedVendorInputs"]
        manifest_keys(adapted, GIT_CRITICAL_VENDOR, exact=True)
        for upstream, identity in adapted.items():
            compiled_hash = identity["compiledSha256"]
            tested_git_vendor(reports, sketch, {upstream: compiled_hash})
            embedded_name = prefix + "src/tab5git/" + upstream
            require(identity.get("adaptation") == "Exact Arduino prefix and relative include substitutions verified" and
                    report["sourceHashes"].get(embedded_name) == identity.get("embeddedSha256") and
                    linked.get("build/libgit2-preflight-0822/" + upstream) == compiled_hash,
                    "Git HTTPS embedded backend adaptation proof is incomplete")
        expected = {
            "HTTPS clone with trusted CA and Basic credentials": 0, "Configure author": 0,
            "Configure email": 0, "HTTPS fetch preserves working HEAD": 0, "HTTPS fast-forward pull": 0,
            "Stage native revision": 0, "Commit native revision": 0, "HTTPS push interoperates with PC Git": 0,
            "Interrupted pack download retains old references": 1, "Retry fetch after interruption succeeds": 0,
            "Stage divergent revision": 0, "Commit divergent revision": 0,
            "Non-fast-forward push refused without changing either HEAD": 1, "Unknown CA rejected": 1,
            "Hostname mismatch rejected": 1, "Bad credentials refused": 1,
            "Cancellation while HTTPS peer delays response": 130,
        }
        rows = report.get("checks", [])
        by_name = {row["name"]: row for row in rows}
        require(len(rows) == len(by_name) and set(expected) <= set(by_name), "Git HTTPS cases missing or duplicated")
        for scenario, code in expected.items():
            row = by_name[scenario]
            require(row.get("exitCode") == code and row.get("retainedBytes") == 0 and
                    0 < row.get("peakTrackedBytes", 0) <= 8 * 1024 * 1024 and row.get("elapsedSeconds", -1) >= 0,
                    "Git HTTPS status/memory evidence failed: " + scenario)
        for scenario in ("Unknown CA rejected", "Hostname mismatch rejected"):
            require(by_name[scenario].get("httpRequests") == 0, "Git sent HTTP before validating TLS: " + scenario)
        for scenario in ("HTTPS clone with trusted CA and Basic credentials", "HTTPS fetch preserves working HEAD",
                         "HTTPS fast-forward pull", "HTTPS push interoperates with PC Git",
                         "Interrupted pack download retains old references", "Retry fetch after interruption succeeds",
                         "Cancellation while HTTPS peer delays response"):
            require(by_name[scenario].get("httpRequests", 0) >= 2, "Git HTTPS scenario did not reach the peer: " + scenario)
        require(report.get("interruptedPackResponses", 0) >= 1 and "localhost" in report.get("tlsNames", []) and
                re.fullmatch(r"[0-9a-f]{64}", report.get("clientSha256", "")) and report.get("limitations"),
                "Git HTTPS interruption, binary identity or limitations missing")
    elif name == "ssh-0822.json":
        require(report.get("physicalDeviceTested") is False, "Unexpected SSH hardware claim")
        hashes(release, report["sources"])
        source_paths = list(sketch.glob("Tab5Ssh*")) + list((sketch / "src/tab5ssh").glob("*"))
        source_paths += [p for p in (release / "tools/ssh-tests").glob("*") if p.suffix in {".py", ".h", ".cpp"}]
        manifest_keys(report["sources"], {p.relative_to(release).as_posix() for p in source_paths if p.is_file()})
        provenance = read_json(sketch / "src/tab5ssh/UPSTREAM.json")
        require(provenance.get("upstream") == "https://github.com/libssh2/libssh2" and
                provenance.get("commit") == "18aebe08962c1878d84c373ad85caff25ec67771" and
                provenance.get("version") == "1.11.2_DEV" and provenance.get("license") == "BSD-3-Clause" and
                provenance.get("patches") and provenance.get("releaseStatus"),
                "SSH dependency provenance or local patch disclosure missing")
        require(digest(sketch / "src/tab5ssh/COPYING") == provenance["upstreamSha256"].get("COPYING"),
                "SSH license differs from declared upstream license")
        native = report["nativeBuild"]
        require(native.get("mbedtlsVersion") == "3.6.5" and
                native.get("mbedtlsCommit") == "ffb280bb63c78bfec1e1ab55040671768c85c923" and
                native.get("sourceUrl") == "https://github.com/espressif/mbedtls/tree/ffb280bb63c78bfec1e1ab55040671768c85c923" and
                native.get("configuration"), "SSH native crypto provenance/configuration missing")
        require(all(re.fullmatch(r"[0-9a-f]{64}", native.get(field, "")) for field in
                    ("mbedtlsSourcesSha256", "mbedtlsBuildInfoSha256", "clientSha256")),
                "SSH native crypto/source/binary identity missing")
        hashes(release, native["appInputs"])
        app_required = {p.relative_to(release).as_posix() for p in (sketch / "src/tab5ssh").glob("*")
                        if p.suffix in {".c", ".h"}}
        app_required.update({sketch.name + "/" + p for p in
                             ("Tab5Ssh.cpp", "Tab5Ssh.h", "Tab5SshPolicy.h", "Tab5TerminalPath.h")})
        app_required.update({"tools/ssh-tests/SshPlatform.h", "tools/ssh-tests/client.cpp", "tools/ssh-tests/build_host.py"})
        manifest_keys(native["appInputs"], app_required)
        require(read_json(reports / "ssh-target-0822.json")["targetMbedtls"].get("version") == native["mbedtlsVersion"],
                "SSH protocol and target builds use different mbedTLS versions")
        rows = report.get("checks", [])
        require(rows and len({row["name"] for row in rows}) == len(rows), "SSH checks missing or duplicated")
        by_name = {row["name"]: row for row in rows}
        for scenario, code in SSH_EXPECTED.items():
            require(scenario in by_name and by_name[scenario].get("returnCode") == code,
                    "Missing or failed SSH scenario: " + scenario)
        for row in rows:
            require(all(row.get(field) == 0 for field in
                        ("retainedSessionBytes", "liveAllocations", "retainedCrtBytes", "prematureTcpWrites")),
                    "SSH allocation/socket lifecycle failure: " + row["name"])
            require(row.get("elapsedSeconds", -1) >= 0 and row.get("peakSessionBytes", -1) >= 0,
                    "Missing SSH measurements: " + row["name"])
        for title, sessions in (("50 sessions in one process retain no CRT allocation", 50),
                                ("20 ECDSA sessions retain no CRT allocation", 20)):
            require(by_name[title].get("sessions", 0) >= sessions, "Missing repeated SSH sessions")
        for prefix, reference, first in (("password session allocation failure ",
                                         "50 sessions in one process retain no CRT allocation", 1),
                                        ("key import/signature allocation failure ",
                                         "20 ECDSA sessions retain no CRT allocation", 30)):
            count = by_name[reference].get("allocationCalls", 0)
            fault_rows = [row for row in rows if row["name"].startswith(prefix)]
            require(count >= first and {row["name"] for row in fault_rows} ==
                    {prefix + str(n) for n in range(first, count + 1)}, "Incomplete SSH OOM sweep: " + prefix)
            require(any(row.get("allocationFailureInjected") is True for row in fault_rows),
                    "SSH OOM sweep did not inject a failure: " + prefix)
    elif name == "ssh-target-0822.json":
        require(report.get("physicalDeviceTested") is False, "Unexpected SSH target hardware claim")
        target_crypto = report["targetMbedtls"]
        require(target_crypto.get("version") == "3.6.5" and
                re.fullmatch(r"[0-9a-f]{64}", target_crypto.get("buildInfoSha256", "")),
                "SSH target SDK crypto version or identity missing")
        required = {p.relative_to(sketch).as_posix() for p in (sketch / "src/tab5ssh").glob("*.c")}
        required.add("Tab5Ssh.cpp")
        rows = report.get("files", [])
        require(len(rows) == len(required) and {row["file"] for row in rows} == required,
                "SSH target compilation inventory differs")
        require(all(row.get("exitCode") == 0 for row in rows), "SSH target object compilation failed")
        hashes(sketch, {row["file"]: row["sha256"] for row in rows})
        inputs = {p.relative_to(sketch).as_posix() for p in (sketch / "src/tab5ssh").glob("*") if p.is_file()}
        inputs.update({"Tab5Ssh.h", "Tab5SshPolicy.h", "Tab5TerminalPath.h"})
        manifest_keys(report["inputs"], inputs)
        hashes(sketch, report["inputs"])
        manifest_keys(report["tests"], {"compile_target.py"}, exact=True)
        hashes(release / "tools/ssh-tests", report["tests"])
        frames = report.get("largestStaticFrames", [])
        require(len(frames) >= 20 and all(row.get("kind") == "static" and row.get("bytes", -1) >= 0
                                        for row in frames) and report.get("stackLimitation"),
                "SSH target stack evidence or its limitations are missing")
    elif name == "libgit2-vendor-0822.json":
        require(report.get("repository") == "https://github.com/libgit2/libgit2" and
                report.get("version") == "v1.9.7" and
                report.get("commit") == "49e408b3208bc3093757a1c2db938d3590f3f412",
                "Unexpected libgit2 dependency provenance")
        rows = git_vendor_rows(reports, sketch)
        root = sketch / "src/tab5git"
        code = {p.relative_to(root).as_posix() for p in root.rglob("*") if p.suffix in {".c", ".h"}}
        recorded = {row["path"] for row in rows.values()}
        require(code <= recorded and sum(p.endswith(".c") for p in recorded) == report.get("units"),
                "Git vendor source inventory is incomplete")
        require((root / "COPYING").is_file() and (root / "UPSTREAM.json").is_file(),
                "Missing Git dependency license/provenance")
        provenance = read_json(root / "UPSTREAM.json")
        require(provenance.get("repository") == report["repository"] and
                provenance.get("tag") == report["version"] and provenance.get("commit") == report["commit"],
                "Git dependency manifests disagree on upstream identity")
        require((root / provenance["manifest"]).resolve() == (reports / name).resolve(),
                "Git provenance points to another or missing source manifest")
        require(provenance.get("patchSummary") and provenance.get("limitations"),
                "Git local changes and validation limitations are missing")
        hashes(root, {provenance["patchFile"]: provenance["patch_sha256"]})
        manifest_keys(provenance["licenses"], {"COPYING", "AUTHORS", "deps/pcre2/LICENCE.md",
                                             "deps/llhttp/LICENSE-MIT", "deps/zlib/LICENSE"})
        hashes(root, provenance["licenses"])
        profile = provenance["profile"]
        require(profile.get("threads") is False and profile.get("mmap") is False and
                profile.get("externalProcesses") is False and profile.get("gitSsh") is False and
                profile.get("sessionHeapLimit") == 8 * 1024 * 1024 and profile.get("fileBufferBytes") == 2048,
                "Git embedded profile differs from the reviewed profile")
        require(profile.get("workerStackBytes") == git_worker_stack(sketch), "Git documented worker stack differs")
    elif name == "git-view-0822.json":
        require(report.get("physicalDeviceTested") is False and report["result"].get("exitCode") == 0 and
                "PASS" in report["result"].get("output", ""), "Git interface checks failed")
        match = re.search(r"PASS (\d+) Git UI production LVGL/lifecycle checks", report["result"].get("output", ""))
        require(match and int(match[1]) >= 281, "Git interface lifecycle/keyboard coverage incomplete")
        manifest_keys(report["sources"], {"Tab5GitView.cpp", "Tab5GitView.h", "Tab5Git.h", "Tab5Focus.cpp",
                      "Tab5UiStyle.cpp", "Tab5TextInput.cpp", "Tab5TerminalBuffer.h", "sdcard/M5Writer/font.ttf"})
        manifest_keys(report["tests"], {"git-view.cpp", "run_git_view.py"}, exact=True)
        hashes(sketch, report["sources"])
        hashes(release / "tools/maps-tests", report["tests"])
    elif name == "sdk-0822.json":
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware claim")
        hashes(release, report["sourceHashes"])
        prefix = sketch.name + "/"
        manifest_keys(report["sourceHashes"], {
            prefix + "tools/native-apps/build.py", prefix + "Tab5AppApi.h",
            prefix + "tools/native-apps/examples/pattern.c", prefix + "tools/native-apps/examples/hash_file.c",
            "examples/micropython/compteur/main.py", "Compiler_Tab5OS.ps1",
            "tools/sdk-tests/run.py", "tools/sdk-tests/test_pattern.c"})
        require(report.get("checks") and all(row.get("passed") is True for row in report["checks"]),
                "SDK checks failed")
        require(len(report.get("elfExamples", [])) >= 2 and all(
                row.get("elfClass") == "ELF32" and row.get("machine") == "RISC-V" and
                row.get("type") == "DYN" and row.get("bytes", 0) > 0 for row in report["elfExamples"]),
                "SDK ELF examples missing or invalid")
        examples = {row["example"]: row for row in report["elfExamples"]}
        require(len(examples) == len(report["elfExamples"]) and {"hash_file", "pattern"} <= set(examples),
                "SDK examples absent or duplicated")
        for example, imports in (("hash_file", ["tab5_app_api_v1"]), ("pattern", ["tab5_app_api_v2"])):
            row = examples[example]
            require(row.get("abi") == "ilp32" and row.get("imports") == imports and
                    row.get("entryPoint") == "tab5_app_main" and
                    re.fullmatch(r"[0-9a-f]{64}", row.get("sha256", "")), "SDK ABI/import evidence incomplete")
        checks = report["checks"]
        require(len(checks) >= 11 and any(row.get("name") == "actual Python template" and
                row.get("events") == 6 and row.get("cleanupOnFailure") is True for row in checks),
                "SDK Python cleanup regression missing")
        require(any(row.get("exitCode") == 1 and any(str(arg).endswith("invalid.c")
                    for arg in row.get("command", [])) for row in checks), "Forbidden ELF import test missing")
        discovery = [row for row in checks if "-CheckOnly" in row.get("command", [])]
        require(len(discovery) >= 4 and sum(row.get("exitCode") == 1 for row in discovery) >= 2 and
                sum(row.get("exitCode") == 0 for row in discovery) >= 2,
                "SDK portable discovery positive/negative tests missing")
    elif name == "elf-lifecycle-0822.json":
        require(report.get("physicalDeviceTested") is False and report.get("checks", 0) >= 22528 and
                report.get("cycles", 0) >= 1024 and report.get("immediateContextCleanup") is True and
                report.get("capsSuspendDeleteOrdering") is True, "ELF context/SDK stack reclamation coverage incomplete")
        manifest_keys(report["sources"], {sketch.name + "/Tab5Executable.cpp", sketch.name + "/Tab5Task.h",
                                         "tools/sdk-tests/elf_lifecycle.cpp", "tools/sdk-tests/run_elf_lifecycle.py"}, exact=True)
        hashes(release, report["sources"])
    elif name == "terminal-network-0819.json":
        require(report.get("sourceUnchangedDuringTests") is True and report.get("checks") and
                all(row.get("exitCode") == 0 for row in report["checks"]), "Historical terminal checks failed")
        hashes(sketch, report["sourceSha256"])
    elif name == "firmware-install-0819.json":
        hashes(sketch, {report["source"]: report["sha256"]})
        require(len(report.get("cases", [])) >= 20 and
                all(row.get("exitCode") == 0 for row in report["cases"]), "Historical firmware cases failed")
    elif name == "pentest-buffer-0819.json":
        hashes(sketch, {"Tab5PentestBuffer.h": report["sourceSha256"]})
        require({row["language"] for row in report["bounds"]} == LOCALES and
                all(row["ports"] == 1024 and row["bytes"] < report["capacity"] for row in report["bounds"]),
                "Historical Pentest buffer bounds failed")
    elif name == "keyboard-shortcuts-0819.json":
        hashes(sketch, report["sources"])
        hashes(sd, {"M5Writer/keyboards.json": report["keyboardSha256"]})
        require(LOCALES | {"en-GB"} <= set(report.get("layouts", [])), "Missing keyboard layouts")
        check_count(report, 655)
    elif name in {"ime-0819.json", "ime-native-0819.json"}:
        hashes(sketch, report["sources"])
        assets = {n: h for n, h in report["assets"].items() if not n.startswith("M5Writer/languages/")}
        hashes(sd, assets)
        require(report.get("physicalDisplayTested") is False, "Unexpected historical hardware claim")
        if name == "ime-0819.json":
            require(report.get("nativeProof") == "ime-native-0819.json" and report.get("glyphProblems") == [],
                    "Historical IME proof is incomplete")
            require(all(row.get("missingGlyphs") == [] for row in report["dictionaries"].values()),
                    "Historical IME dictionary glyphs failed")
    else:
        raise ValueError("Unchecked report: " + name)


def collect_evidence(release):
    sketch = release / f"Tab5OS_Arduino_v{VERSION}_Ecriture"
    reports = release / "tools/verification"
    require(release.name == f"Tab5OS_Ecriture_v{VERSION}" and sketch.is_dir(), "Wrong release root")
    sources = source_manifest(sketch)
    dependencies = dependency_manifest(sketch)
    verifier = digest(Path(__file__))
    require(sources, "Empty source manifest")
    checks, errors, report_hashes = [], [], {}
    for name in (*CURRENT_REPORTS, *HISTORICAL_REPORTS):
        try:
            report_hashes[name] = digest(reports / name)
            checked_report(name, read_json(reports / name), release, sketch, reports)
            checks.append({"report": name, "passed": True, "historical": name in HISTORICAL_REPORTS})
        except (OSError, ValueError, KeyError, TypeError, SyntaxError) as error:
            errors.append(name + ": " + str(error))
            checks.append({"report": name, "passed": False, "error": str(error)})
    if source_manifest(sketch) != sources:
        errors.append("Sources changed during feature verification")
    if dependency_manifest(sketch) != dependencies:
        errors.append("Dependency sources changed during feature verification")
    if digest(Path(__file__)) != verifier:
        errors.append("Verifier changed during feature verification")
    for name, expected in report_hashes.items():
        if not (reports / name).is_file() or digest(reports / name) != expected:
            errors.append("Report changed during feature verification: " + name)
    # Informational only: compiled host test binaries are not source-package
    # deliverables, nor evidence of identical P4 binary/SDK allocation behavior.
    host_binary = release.parent / "libgit2-host-0822/git2.lib"
    environment = {"gitInitOomHostLibraryAvailable": host_binary.is_file(),
                   "scope": "Optional local host artifact identity; portable verdict uses tested source and vendor provenance."}
    if host_binary.is_file() and (reports / "git-init-oom-0822.json").is_file():
        try:
            recorded = read_json(reports / "git-init-oom-0822.json").get("hostLibrarySha256")
            environment["gitInitOomHostLibraryMatchesRecorded"] = digest(host_binary) == recorded
        except (OSError, ValueError, TypeError):
            environment["gitInitOomHostLibraryMatchesRecorded"] = None
    return dict(version=VERSION, checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                passed=not errors, physicalDeviceTested=False,
                scope="Current native regressions, target syntax and resources; unchanged 0.8.19 evidence retains its original scope and date. No physical display, Wi-Fi, USB, firmware flash or VPN tunnel test.",
                sources=sources, dependencies=dependencies, verifierSha256=verifier,
                reports=report_hashes, checks=checks, errors=errors, environment=environment)


def main():
    release = Path(__file__).resolve().parents[1]
    result = collect_evidence(release)
    destination = release / "tools/verification/features-0822.json"
    temporary = destination.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(destination)
    print(json.dumps({"passed": result["passed"], "sourceUnits": len(result["sources"]),
                      "reports": len(result["reports"]), "errors": result["errors"]}, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
