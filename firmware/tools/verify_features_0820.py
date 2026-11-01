"""Verify current 0.8.20 evidence without relabelling historical test results.

Run after the final source freeze and the individual report generators. A stale
or missing report produces a failed features-0820.json and a nonzero exit code.
Compilation, firmware validation and packaging have their own additional gates.
"""
from __future__ import annotations

import datetime
import hashlib
import json
from pathlib import Path
import re

VERSION = "0.8.20"
CURRENT_REPORTS = (
    "wifi-transitions-0820.json", "network-gate-0820.json",
    "directory-picker-0820.json", "ui-controls-0820.json", "calculator-0820.json",
    "glyphs-0820.json", "arabic-0820.json", "languages-0820.json",
    "delta-languages-0820.json", "ui-locale-calls-0820.json",
    "syntax-0820.json", "resources-0820.json",
)
# Byte-identical copies of the released 0.8.19 reports. Their source hashes must
# also still match; dates, labels and historical language-pack hashes stay intact.
HISTORICAL_REPORTS = {
    "terminal-network-0819.json": "bf04dbe5b1440c7606873f552d5f62bba2a5886ebf99f19845360d78d8f2debd",
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
                "Tab5UiStyle.cpp", "Tab5ThemePreview.cpp"}


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


def source_manifest(sketch):
    return {p.name: digest(p) for p in sorted(sketch.iterdir())
            if p.is_file() and p.suffix in {".ino", ".cpp", ".h", ".inc"}}


def check_count(report, minimum):
    match = re.search(r"PASS (\d+) (?:real decoder )?checks", report.get("output", ""))
    require(match and int(match[1]) >= minimum,
            "Missing successful checks: expected at least " + str(minimum))


def checked_report(name, report, release, sketch, reports):
    sd = sketch / "sdcard"
    if name in HISTORICAL_REPORTS:
        require(digest(reports / name) == HISTORICAL_REPORTS[name],
                "Historical report was modified: " + name)
    elif "version" in report or "release" in report:
        require(report.get("version", report.get("release")) == VERSION,
                "Wrong release in " + name)
    success = report.get("pass") if name == "arabic-0820.json" else report.get("passed")
    if name == "resources-0820.json":
        success = report.get("result") == "PASS"
    require(success is True, "Report did not pass: " + name)

    if name == "wifi-transitions-0820.json":
        current, baseline = report["current"], report["baseline_0_8_19"]
        require(current.get("passed") is True and current.get("exit_code") == 0,
                "Current Wi-Fi transitions did not pass")
        require(baseline.get("expected_failure_observed") is True and
                baseline.get("exit_code") == 1 and
                baseline.get("output", "").count("EXPECTED-FAIL:") == 3,
                "Missing the three reproduced 0.8.19 Wi-Fi failures")
        require(current.get("sources") == report.get("sources"), "Wi-Fi manifests disagree")
        hashes(sketch, report["sources"])
    elif name == "network-gate-0820.json":
        require(report.get("status") == "PASS", "Network gate did not pass")
        hashes(sketch, report["sources"])
        require(digest(sketch / "Tab5NetworkGate.cpp") == report.get("source_sha256"),
                "Network gate source changed")
    elif name == "directory-picker-0820.json":
        require(report.get("exitCode") == 0, "Directory/picker test failed")
        check_count(report, 540)
        hashes(sketch, report["sources"])
    elif name in {"ui-controls-0820.json", "calculator-0820.json"}:
        if name == "ui-controls-0820.json":
            require(report.get("exitCode") == 0, "Native UI test failed")
        check_count(report, 188 if name.startswith("ui-") else 2082)
        hashes(release, report["sources"])
    elif name == "glyphs-0820.json":
        hashes(sketch, report["sources"])
        hashes(sd, report["assets"])
        require(set(report["packs"]) == LOCALES and
                all(p.get("missingGlyphs") == [] for p in report["packs"].values()),
                "Current pack glyph coverage is incomplete")
        require(all(report.get("calculatorSymbols", {}).get(c) is True for c in "π√×÷−"),
                "Calculator glyph evidence is incomplete")
        require(report.get("physicalDisplayTested") is False, "Unexpected hardware-test claim")
    elif name == "arabic-0820.json":
        require(report.get("unsupportedShapedGlyphs") == {} and
                report.get("physicalDisplayTested") is False, "Arabic shaping evidence failed")
        require(digest(sd / "M5Writer/languages/ar.json") == report.get("packSha256") and
                digest(sd / "M5Writer/font.ttf") == report.get("fontSha256"),
                "Arabic pack or font changed after shaping tests")
        require(report.get("sampleCount", 0) >= report.get("arabicPackEntries", 0) > 0,
                "Incomplete Arabic shaping samples")
    elif name == "languages-0820.json":
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
    elif name == "delta-languages-0820.json":
        hashes(release / "tools/localization", report["deltaFiles"])
        require(set(report["packs"]) == LOCALES, "Incomplete new-key translation inventory")
        for locale, audit in report["packs"].items():
            require(audit.get("translatedNewKeys") == report.get("newSourceKeys") and
                    digest(sd / "M5Writer/languages" / (locale + ".json")) == audit.get("sha256"),
                    "Current translation delta differs: " + locale)
    elif name == "ui-locale-calls-0820.json":
        require(report.get("missingKeys") == {}, "Untranslated literal callsites")
        hashes(sketch, report["sourceHashes"])
        # The literal-call audit excludes the two generated English tables.
        # They are still bound by this gate's complete top-level source manifest.
        audited_units = set(source_manifest(sketch)) - {"Tab5EnglishBitmap.h", "Tab5EnglishFallback.h"}
        require(set(report["sourceHashes"]) == audited_units,
                "Locale audit does not cover the current source unit inventory")
        languages = read_json(reports / "languages-0820.json")
        require(report.get("referenceEntries") == languages.get("referenceEntries"),
                "Locale audit reference changed")
    elif name == "syntax-0820.json":
        rows = report.get("files", [])
        names = [row["name"] for row in rows]
        require(SYNTAX_UNITS <= set(names) and len(names) == len(set(names)),
                "Missing or duplicate current target-syntax units")
        require(report.get("physicalDeviceTested") is False, "Unexpected hardware-test claim")
        for row in rows:
            require(row.get("exitCode") == 0 and row.get("sourceUnchanged") is True,
                    "Target syntax failed or source changed: " + row["name"])
            hashes(sketch, {row["name"]: row["sha256"]})
    elif name == "resources-0820.json":
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
    for name, expected in report_hashes.items():
        if not (reports / name).is_file() or digest(reports / name) != expected:
            errors.append("Report changed during feature verification: " + name)
    return dict(version=VERSION, checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                passed=not errors, physicalDeviceTested=False,
                scope="Current native regressions, target syntax and resources; unchanged 0.8.19 evidence retains its original scope and date. No physical display, Wi-Fi, USB, firmware flash or VPN tunnel test.",
                sources=sources, reports=report_hashes, checks=checks, errors=errors)


def main():
    release = Path(__file__).resolve().parents[1]
    result = collect_evidence(release)
    destination = release / "tools/verification/features-0820.json"
    temporary = destination.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(destination)
    print(json.dumps({"passed": result["passed"], "sourceUnits": len(result["sources"]),
                      "reports": len(result["reports"]), "errors": result["errors"]}, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
