"""Check 0.8.23 evidence without relabelling old test executions.

Unchanged .22 reports retain their bytes, dates and scope. Their production
sources AND harness hashes must match this release before reuse. Only the
versioned sketch directory component is mapped to its new location. A changed
source is never accepted by copying or rewriting an old source hash.
"""
from __future__ import annotations

import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import re

VERSION = "0.8.23"
BASELINE = "0.8.22"
HISTORY_SHA256 = "eca9c070c989e2ced2466acaae979481ffac219c9940bcc989d35e9919dcb34f"
REPLACED = {
    "pentest-lifecycle-0822.json": ("pentest-elf-0823.json", "app-panel-0823.json"),
    "pentest-ui-0822.json": ("pentest-elf-0823.json", "app-panel-0823.json"),
    "pentest-buffer-0819.json": ("pentest-elf-0823.json",),
}
# These tests concern requested changes and cannot silently fall back to .22.
REQUIRE_CURRENT = {
    "maps-0822.json", "gamepad-0822.json", "git-view-0822.json",
    "elf-lifecycle-0822.json", "resource-inventory-0822.json",
    "syntax-0822.json", "resources-0822.json", "languages-0822.json",
    "delta-languages-0822.json", "ui-locale-calls-0822.json",
}
EXTRA_REPORTS = ("pentest-elf-0823.json", "app-panel-0823.json", "elf-resources-0823.json", "context-menu-0823.json",
                 "gamepad-pipeline-0823.json", "gamepad-settings-0823.json", "elf-api5-0823.json",
                 "explorer-0823.json", "git-close-baseline-0823.json", "app-panel-ui-0823.json",
                 "consoles-ui-0823.json")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def hashes(base, manifest, required=()):
    require(isinstance(manifest, dict) and manifest, "Missing hash manifest")
    normalized = {name.replace("\\", "/"): value for name, value in manifest.items()}
    require(len(normalized) == len(manifest), "Duplicate manifest paths")
    require(set(required) <= set(normalized), "Incomplete manifest: " + ", ".join(sorted(set(required)-set(normalized))))
    for name, expected in manifest.items():
        require(re.fullmatch(r"[a-f0-9]{64}", expected) and digest(safe_path(base, name)) == expected,
                "Hash mismatch: " + name)


def safe_path(base, name):
    require(isinstance(name, str) and name, "Empty manifest path")
    path = (base / name.replace("\\", "/")).resolve()
    require(path.is_relative_to(base.resolve()), "Unsafe manifest path: " + name)
    return path


def selected_report_path(release, name):
    reports = release / "tools/verification"
    current = reports / re.sub(r"-08\d{2}(?=\.)", "-0823", name)
    if current.is_file() or name in REQUIRE_CURRENT:
        return current
    return reports / name


def read_selected(release, name):
    return read_json(selected_report_path(release, name))


def relocate_path(value):
    """Relocate a path, never its content, timestamp, version or digest."""
    old = "Tab5OS_Arduino_v0.8.22_Ecriture"
    new = "Tab5OS_Arduino_v0.8.23_Ecriture"
    if value == old + ".ino":
        return new + ".ino"
    if value.startswith(old + "/") or value.startswith(old + "\\"):
        return (new + value[len(old):]).replace("/" + old + ".ino", "/" + new + ".ino").replace("\\" + old + ".ino", "\\" + new + ".ino")
    return value


def relocated(data):
    if isinstance(data, dict):
        result = {relocate_path(k): relocated(v) for k, v in data.items()}
        require(len(result) == len(data), "Duplicate relocated manifest paths")
        return result
    if isinstance(data, list):
        return [relocated(v) for v in data]
    return relocate_path(data) if isinstance(data, str) else data


class ReportDirectory:
    def __init__(self, release):
        self.release = release

    def __truediv__(self, name):
        return selected_report_path(self.release, name)


def load_rules(release, history):
    path = release / "tools/verify_features_0822.py"
    require(digest(path) == history["verifierSha256"], "Historical validation rules changed")
    spec = importlib.util.spec_from_file_location("tab5_rules_0822", path)
    rules = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(rules)
    rules.read_json = lambda path: relocated(read_json(path))
    rules.SYNTAX_UNITS = {relocate_path(n) for n in rules.SYNTAX_UNITS}
    rules.SYNTAX_UNITS.discard("Tab5Pentest.cpp")
    rules.SYNTAX_UNITS.update({"Tab5ContextMenu.cpp", "Tab5AppPanel.cpp", "Tab5ElfValidate.cpp", "Tab5NativeView.cpp"})
    return rules


def checked_extra(name, report, release, sketch):
    require(report.get("version") == VERSION and report.get("passed") is True and
            report.get("physicalDeviceTested") is False, "New regression failed or scope missing: " + name)
    prefix = sketch.name + "/"
    if name == "context-menu-0823.json":
        expected_cases = {"mouse-deferred-and-theme", "keyboard-shortcuts-and-navigation",
                          "outside-escape-and-owner-deletion", "callback-reopens-menu-and-local-items",
                          "textarea-copy-cut-paste-undo-selection", "readonly-console-clear-and-bind-lifetime",
                          "password-and-disabled-field", "bounded-menu-invalid-input-and-binding-OOM",
                          "repeated-menu-output-lifecycle"}
        require(report.get("exitCode") == 0 and report.get("checks", 0) >= 2719 and
                report.get("cycles", 0) >= 512 and report.get("historyBytes") == 0 and
                report.get("sourceStableDuringTests") is True and report.get("lvglVersion") == "9.6.0" and
                set(report.get("cases", [])) == expected_cases and report.get("groups") == len(expected_cases),
                "Context-menu real LVGL/lifecycle/keyboard/OOM coverage incomplete")
        require(re.fullmatch(r"[0-9a-f]{64}", report.get("lvglLibrarySha256", "")) and
                report.get("lvglSourceManifest", {}).get("sources") and report.get("limitations"),
                "Context-menu LVGL identity or test limits missing")
        hashes(sketch, report["sources"], {"Tab5ContextMenu.cpp", "Tab5ContextMenu.h", "Tab5Focus.cpp", "Tab5Focus.h",
               "Tab5TextInput.cpp", "Tab5TextInput.h", "Tab5UiStyle.cpp", "Tab5UiStyle.h", "WriterKeys.h",
               "WriterDocument.cpp", "WriterDocument.h"})
        hashes(release / "tools/context-menu-tests", report["tests"], {"run.py", "context_menu.cpp"})
        match = re.search(r"PASS (\d+) context-menu checks; groups=(\d+); cycles=(\d+); history_bytes=(\d+); LVGL=(\S+);", report.get("output", ""))
        require(match and [int(match[i]) for i in range(1, 5)] == [report[k] for k in ("checks", "groups", "cycles", "historyBytes")]
                and match[5] == report["lvglVersion"], "Context-menu execution output absent or disagrees with counters")
    elif name == "consoles-ui-0823.json":
        match = re.search(r"PASS (\d+) console UI checks; (\d+) open/close sessions; Terminal/Python/ELF button, Ctrl\+L, context, selection and production refresh", report.get("output", ""))
        require(report.get("exitCode") == 0 and report.get("sourceStableDuringTests") is True and
                match and int(match[1]) >= 1044 and int(match[2]) >= 36 and report.get("scope") and
                re.fullmatch(r"[a-f0-9]{64}", report.get("lvglLibrarySha256", "")),
                "Console clear/refresh/selection/lifecycle evidence incomplete")
        hashes(sketch, report["sources"], {"Tab5Tools.cpp", "Tab5Tools.h", "Tab5ContextMenu.cpp", "Tab5ContextMenu.h",
               "Tab5TextInput.cpp", "Tab5TextInput.h", "Tab5Focus.cpp", "Tab5Focus.h", "Tab5UiStyle.cpp",
               "Tab5UiStyle.h", "WriterDocument.cpp", "WriterDocument.h", "WriterKeys.h"})
        hashes(release / "tools/context-menu-tests", report["tests"], {"run_consoles.py", "consoles.cpp"})
    elif name == "elf-resources-0823.json":
        require(report.get("checks", 0) >= 175156 and report.get("cycles", 0) >= 1024 and
                report.get("allocationFailuresInjected", 0) >= 1026 and report.get("trackedPeakBytes", 0) > 0 and
                all(report.get(k) == 0 for k in ("trackedRetainedBytes", "filesRetained", "storageLeasesRetained")) and
                report.get("imageCacheDrops", 0) >= 1024 and report.get("scope"),
                "ELF cleanup/quota/cache/OOM evidence incomplete")
        hashes(release, report["sources"], {prefix + n for n in ("Tab5Executable.cpp", "Tab5NativeView.cpp",
               "src/tab5_elf/esp_elf.c", "Tab5ResourceManager.cpp", "Tab5ResourceManager.h", "Tab5ResourcePolicy.h", "Tab5AppApi.h")} |
               {"tools/sdk-tests/elf_resources.cpp", "tools/sdk-tests/run_elf_resources.py"})
    elif name == "gamepad-pipeline-0823.json":
        require(report.get("exitCode") == 0 and report.get("extractedFunctions") ==
                ["releasePad", "interfaceEvent", "usbGameButtons", "usbGamepadInfo"], "USB production callback coverage differs")
        match = re.search(r"PASS: (\d+) production callback/API assertions; HID and XInput without F1, (\d+) press/release cycles", report.get("output", ""))
        require(match and int(match[1]) >= 20036 and int(match[2]) >= 4000, "USB F1-independent callback/input tests incomplete")
        hashes(release, report["sources"], {prefix+n for n in ("WriterUsb.cpp", "WriterGamepad.cpp", "WriterGamepad.h",
               "WriterHid.cpp", "WriterHid.h", "WriterMouse.cpp", "WriterMouse.h")} |
               {"tools/usb-gamepad-tests/run_pipeline.py", "tools/usb-gamepad-tests/test_pipeline.cpp"})
    elif name == "gamepad-settings-0823.json":
        match = re.search(r"PASS: (\d+) actual LVGL settings assertions", report.get("output", ""))
        require(report.get("exitCode") == 0 and match and int(match[1]) >= 59 and
                re.fullmatch(r"[0-9a-f]{64}", report.get("lvglArchiveSha256", "")), "USB settings LVGL evidence incomplete")
        hashes(release, report["sources"], {prefix+n for n in ("Tab5GamepadSettings.inc", "WriterGamepad.h", "WriterKeys.h")} |
               {"tools/usb-gamepad-tests/run_settings.py", "tools/usb-gamepad-tests/test_settings.cpp"})
    elif name == "elf-api5-0823.json":
        require(report.get("checks", 0) >= 32 and all(report.get(k) is True for k in
                ("abiPrefixPreserved", "workerOnly", "synchronousNetworkCallback", "legacyNoPanelLock", "noGameOverlayForApi5"))
                and report.get("scope"), "ELF API5 prefix/worker/network/cancellation evidence incomplete")
        hashes(release, report["sources"], {prefix+n for n in ("Tab5Executable.cpp", "Tab5AppApi.h", "Tab5AppPanel.h")} |
               {"tools/sdk-tests/run_elf_api5.py", "tools/sdk-tests/elf_api5.cpp"})
    elif name == "app-panel-0823.json":
        match = re.search(r"PASS checks=(\d+) sessions=(\d+) concurrentWrites=(\d+) retainedAppAllocations=0 retainedBrokerAllocations=0", report.get("output", ""))
        require(report.get("exitCode") == 0 and match and int(match[1]) >= 19128 and int(match[2]) >= 1000 and int(match[3]) >= 10000,
                "Pentest/broker session/concurrent clear coverage incomplete")
        hashes(sketch, report["sources"], {"Tab5AppPanel.cpp", "Tab5AppPanel.h", "Tab5ConsoleBuffer.h", "Tab5AppApi.h",
                                         "tools/native-apps/pentest/pentest.c"})
        hashes(release / "tools/pentest-elf-tests", report["tests"], {"run.py", "panel.cpp"})
    elif name == "pentest-elf-0823.json":
        import struct
        hashes(sketch, report["sources"], {"tools/native-apps/pentest/pentest.c", "tools/native-apps/build.py", "Tab5AppApi.h",
               "Tab5ElfValidate.cpp", "Tab5ElfValidate.h", "sdcard/Applications/Pentest.app.elf",
               "sdcard/Applications/Pentest.app.elf.launch.json"})
        hashes(release / "tools/pentest-elf-tests", report["tests"], {"run_elf.py", "validate.cpp"})
        data = (sketch / "sdcard/Applications/Pentest.app.elf").read_bytes()
        require(report.get("exitCode") == 0 and report.get("bytes") == len(data) and len(data) > 52 and
                data[:6] == b"\x7fELF\x01\x01" and struct.unpack_from("<HH", data, 16) == (3, 243) and
                struct.unpack_from("<I", data, 36)[0] & 6 == 0 and report.get("imports") == ["tab5_app_api_v5"] and
                "PASS real shipped ELF accepted; ABI5-only import; stack=32768; unknown ABI6 import rejected" in report.get("output", ""),
                "Pentest actual RV32 ABI5/negative ABI6 ELF validation incomplete")
    elif name == "app-panel-ui-0823.json":
        match = re.search(r"PASS (\d+) real-LVGL ELF panel opening/modes/report/selection/RAZ/context-menu/close checks", report.get("output", ""))
        require(report.get("exitCode") == 0 and match and int(match[1]) >= 625, "ELF panel real LVGL RAZ/lifecycle coverage incomplete")
        hashes(sketch, report["sources"], {"Tab5Tools.cpp", "Tab5ContextMenu.cpp", "Tab5ContextMenu.h", "Tab5TextInput.cpp",
               "Tab5TextInput.h", "Tab5Focus.cpp", "Tab5Focus.h", "Tab5UiStyle.cpp", "Tab5UiStyle.h", "WriterDocument.cpp",
               "WriterDocument.h", "WriterKeys.h"})
        hashes(release / "tools/pentest-elf-tests", report["tests"], {"run_ui.py", "ui.cpp"})
    elif name == "explorer-0823.json":
        hashes(sketch, report["sources"], {sketch.name+".ino", "Tab5ExplorerMenus.inc", "Tab5GitView.h", "Tab5Focus.cpp",
               "Tab5TextInput.cpp", "Tab5UiStyle.cpp", "Tab5ContextMenu.cpp", "Tab5ContextMenu.h", "Tab5FileSelection.h"})
        hashes(release / "tools/maps-tests", report["tests"], {"run_explorer.py", "explorer.cpp"})
        rows = report.get("results", [])
        require(len(rows) == 2 and all(row.get("exitCode") == 0 for row in rows), "Explorer regression execution failed")
        baseline = next((row for row in rows if row.get("baseline") is True), {})
        current = next((row for row in rows if row.get("baseline") is False), {})
        match = re.search(r"PASS (\d+) Explorer production controllers/menu checks", current.get("output", ""))
        old = read_json(release / "tools/verification/features-0822.json")
        require(match and int(match[1]) >= 819 and "2 expected failures" in baseline.get("output", "") and
                "FAIL Explorer mkdir must not load or rebuild the inactive audio app" in baseline["output"] and
                "FAIL DELETE keyboard callbacks cannot rediscover a retiring dialog" in baseline["output"] and
                current.get("controllerSha256") == digest(sketch / (sketch.name+".ino")) and
                baseline.get("controllerSha256") == old["sources"]["Tab5OS_Arduino_v0.8.22_Ecriture.ino"],
                "Explorer current/source identity or .22 reproduced errors incomplete")
    elif name == "git-close-baseline-0823.json":
        old = read_json(release / "tools/verification/git-view-0822.json")
        require(report.get("baselineSha256") == old["sources"]["Tab5GitView.cpp"] and report.get("exitCode") == 1 and
                "AddressSanitizer" in report.get("output", "") and "cleanup_event_list_core" in report["output"] and
                report["output"].count("LVGL control: ordinary parent deletion during event is supported = 1") == 25,
                "Git original event-lifetime failure or LVGL controls missing")
        # The runner must bind its harness and real ASan compilation evidence.
        hashes(sketch, report["sources"], {"Tab5GitView.h", "Tab5Git.h", "Tab5Focus.cpp", "Tab5Focus.h",
               "Tab5UiStyle.cpp", "Tab5TextInput.cpp", "Tab5ContextMenu.cpp", "Tab5ContextMenu.h",
               "Tab5TerminalBuffer.h", "WriterDocument.cpp"})
        hashes(release / "tools/maps-tests", report["tests"], {"run_git_view.py", "git-view.cpp"})
        compilation = report.get("compilation", {})
        require(compilation.get("exitCode") == 0 and "/fsanitize=address" in compilation.get("argv", []) and
                "/DTAB5_TEST_BASELINE=1" in compilation["argv"] and
                all(re.fullmatch(r"[a-f0-9]{64}", compilation.get(key, "")) for key in
                    ("generatedHarnessSha256", "logSha256")) and
                re.fullmatch(r"[a-f0-9]{64}", compilation.get("lvglLibrary", {}).get("sha256", "")) and
                set(key.replace("\\", "/") for key in compilation.get("instrumentedLvgl", {})) ==
                    {"src/misc/lv_event.c", "src/core/lv_obj_event.c"} and
                all(re.fullmatch(r"[a-f0-9]{64}", value) for value in compilation["instrumentedLvgl"].values()) and
                compilation.get("instrumentation"), "Git baseline sanitizer build provenance missing")
    else:
        # Each new regression gets its specific contract before it can pass.
        raise ValueError("New regression contract not yet finalized: " + name)


def checked_gamepad(report, release, sketch):
    require(report.get("version") == VERSION and report.get("passed") is True and report.get("physicalDeviceTested") is False, "USB report failed or scope incorrect")
    hashes(sketch, report["files"], {"src/usb_hid/hid_host.c", "src/usb_hid/input_interface.h", "src/usb_hid/gamepad_transport.h",
                                  "WriterUsb.cpp", "WriterGamepad.cpp", "WriterHid.cpp", "WriterMouse.cpp"})
    hashes(release / "tools/usb-gamepad-tests", report["harness"], {"host_stub.h", "run.py", "run_host.py", "run_msvc.cmd",
                                                                 "test_gamepad.cpp", "test_host.c", "test_interfaces.cpp"})
    current, baseline = report["current"], report["baseline"]
    match = re.search(r"PASS: (\d+) USB lifecycle assertions; 0 failures; retained allocations 0; claims 0", current.get("result", ""))
    old = read_json(release / "tools/verification/gamepad-0822.json")
    require(current.get("exitCode") == 0 and match and int(match[1]) >= 46089 and
            "hid_host_xinput_initialize" in current.get("extractedFunctions", []) and
            current.get("sourceSha256") == digest(sketch / "src/usb_hid/hid_host.c") and
            baseline.get("exitCode") == 1 and "2 failures" in baseline.get("result", "") and
            baseline.get("sourceSha256") == old["current"]["sourceSha256"] and
            "PASS: 4069" in report.get("enumeration", "") and "PASS:" in report.get("packets", ""),
            "USB initialization regression / .22 baseline evidence incomplete")


def checked_maps(report, release, sketch):
    require(report.get("version") == VERSION and report.get("passed") is True and
            report.get("physicalDeviceTested") is False, "Current map execution or scope missing")
    hashes(release, report["sources"])
    hashes(release, report["tests"], {"tools/maps-tests/run.py", "tools/maps-tests/imports.py",
           "tools/maps-tests/view.cpp", "tools/maps-tests/native-import.cpp"})
    rows = report.get("results", [])
    require(len(rows) == 3 and all(row.get("exitCode") == 0 for row in rows), "Current map test execution failed")
    for row, pattern, minimum in zip(rows, (
            r"PASS (\d+) production zoom/LVGL/geometry/raster checks",
            r"PASS (\d+) native import/planner/rate/actual LVGL lifecycle checks",
            r"PASS (\d+) production importer and PC controller checks"), (1744, 6083, 597)):
        match = re.search(pattern, row.get("output", ""))
        require(match and int(match[1]) >= minimum, "Map geometry, native import or PC import coverage incomplete")
    main = (sketch / (sketch.name + ".ino")).read_text(encoding="utf-8")
    require(report.get("controllers") and report.get("method"), "Map controller identity or method missing")
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


def collect_evidence(release):
    release = release.resolve()
    sketch = release / f"Tab5OS_Arduino_v{VERSION}_Ecriture"
    reports = release / "tools/verification"
    require(release.name == f"Tab5OS_Ecriture_v{VERSION}" and sketch.is_dir(), "Wrong release root")
    history_path = reports / "history-0822/index.json"
    require(digest(history_path) == HISTORY_SHA256, "Historical evidence index changed")
    history = read_json(history_path)
    require(history["baselineVersion"] == BASELINE, "Wrong historical evidence baseline")
    rules = load_rules(release, history)
    sources = rules.source_manifest(sketch)
    dependencies = rules.dependency_manifest(sketch)
    verifier = digest(Path(__file__))
    report_hashes = {"history-0822/index.json": HISTORY_SHA256}
    for name, expected in history["reports"].items():
        require(digest(safe_path(reports, name)) == expected, "Historical evidence bytes changed: " + name)
        report_hashes[name] = expected
    checks, errors = [], []
    selected = ReportDirectory(release)
    names = [n for n in (*rules.CURRENT_REPORTS, *rules.HISTORICAL_REPORTS) if n not in REPLACED]
    for name in names:
        path = selected / name
        actual_name = path.name
        historical = actual_name == name
        try:
            report_hashes[actual_name] = digest(path)
            raw = read_json(path)
            if historical and name in rules.CURRENT_REPORTS:
                require(report_hashes[actual_name] == history["reports"][name], "Historical report bytes changed: " + name)
            elif not historical:
                require(raw.get("version", raw.get("release")) == VERSION,
                        "A new report must explicitly identify 0.8.23: " + actual_name)
                require(report_hashes[actual_name] not in history["reports"].values(), "An old report was renamed as current")
            rules.VERSION = BASELINE if historical else VERSION
            old_hash = rules.HISTORICAL_REPORTS.pop(name, None) if not historical else None
            try:
                if not historical and name == "gamepad-0822.json":
                    checked_gamepad(raw, release, sketch)
                elif not historical and name == "maps-0822.json":
                    checked_maps(raw, release, sketch)
                else:
                    rules.checked_report(name, relocated(raw), release, sketch, selected)
                if not historical and name == "elf-lifecycle-0822.json":
                    require(raw.get("checks", 0) >= 25856 and raw.get("audioReleaseContention") is True and
                            raw.get("baseline", {}).get("expectedFailure") is True and raw["baseline"].get("exitCode") == 2 and
                            raw["baseline"].get("version") == BASELINE, "ELF audio-release regression or .22 baseline missing")
            finally:
                if old_hash is not None:
                    rules.HISTORICAL_REPORTS[name] = old_hash
            checks.append(dict(report=actual_name, passed=True, historical=historical,
                               originVersion=BASELINE if historical and name in rules.CURRENT_REPORTS else raw.get("version", raw.get("release")),
                               reusedOnlyAfterSourceAndHarnessHashCheck=historical))
        except (OSError, ValueError, KeyError, TypeError, SyntaxError) as error:
            errors.append(actual_name + ": " + str(error))
            checks.append(dict(report=actual_name, passed=False, historical=historical, error=str(error)))
    for name in EXTRA_REPORTS:
        try:
            path = reports / name
            report_hashes[name] = digest(path)
            raw = read_json(path)
            require(raw.get("version") == VERSION and raw.get("passed") is True and
                    raw.get("physicalDeviceTested") is False, "New regression failed or scope missing: " + name)
            checked_extra(name, raw, release, sketch)
            checks.append(dict(report=name, passed=True, historical=False, originVersion=VERSION))
        except (OSError, ValueError, KeyError, TypeError, SyntaxError) as error:
            errors.append(name + ": " + str(error))
            checks.append(dict(report=name, passed=False, historical=False, error=str(error)))
    if rules.source_manifest(sketch) != sources:
        errors.append("Sources changed during verification")
    if rules.dependency_manifest(sketch) != dependencies:
        errors.append("Dependencies changed during verification")
    if digest(Path(__file__)) != verifier:
        errors.append("Verifier changed during verification")
    for name, expected in report_hashes.items():
        if digest(safe_path(reports, name)) != expected:
            errors.append("Report changed during verification: " + name)
    return dict(version=VERSION, checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                passed=not errors, physicalDeviceTested=False, sources=sources, dependencies=dependencies,
                verifierSha256=verifier, ruleSetSha256=history["verifierSha256"], reports=report_hashes,
                checks=checks, errors=errors, replacedEvidence=REPLACED,
                scope="Current regression executions plus byte-identical historical reports whose complete recorded source and harness hashes still match. No physical Tab5 test or firmware flash.",
                baselineArchiveSha256=history["baselineArchiveSha256"])


def main():
    release = Path(__file__).resolve().parents[1]
    result = collect_evidence(release)
    target = release / "tools/verification/features-0823.json"
    temporary = target.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(target)
    print(json.dumps({"passed": result["passed"], "sourceUnits": len(result["sources"]),
                      "reports": len(result["checks"]), "errors": result["errors"]}, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
